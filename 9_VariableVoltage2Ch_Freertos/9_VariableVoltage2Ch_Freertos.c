// 파일이름	:	9_VariableVoltage2Ch_Freertos.c
// 대상장치	:	TMS320F28X EVM V2, TMS320F28P65x(F28P650DK9 산업용 / F28P659DK8-Q1 차량용) 모듈
// 파일버전	:	1.00
// 갱신이력	:	2026-09-18, 버전 1.00
// 예제설명	:

//************************************************************************************************************************************************************************
//
// 본 예제는 TMS320F28P65x 모듈이 탑재된 TMS320F28X 개발보드(EVM) V2를 대상으로 하고 있으며,
// TMS320F28P65x 칩의 ADC 회로로 개발보드의 가변 전압 출력 회로를 읽습니다.
//
// TMS320F28X 개발보드 V2의 '(9) 가변 전압 출력 2채널' 회로는 로터리 가변저항(포텐셔미터)으로
// 0.0V~3.3V 아날로그 전압을 만들어내는 회로입니다.
//
// 본 예제는 FreeRTOS 커널 위에서 하나의 정적(Static, 힙 미사용) 태스크로 동작합니다:
//   ADC_Task (우선순위 1, 50ms 주기): ADCINA0/A1을 소프트웨어 트리거로 변환하고
//   전역 변수에 12비트 결과와 mV 환산값을 저장합니다.
//
// a. 배선 안내:
//
//		>> 1채널: 가변전압 출력 1채널 ──▶ B Side 111번 핀 (ADCINA0)
//		>> 2채널: 가변전압 출력 2채널 ──▶ B Side 109번 핀 (ADCINA1)
//
//	 (Bitfield 예제, DriverLib 예제와 완전히 동일한 점퍼 배선으로 공용할 수 있습니다.)
//
// 예제 폴더의 CCS Expressions 창에 아래 변수를 등록해두면 동작을 실시간으로 확인할 수 있습니다.
// --> adcResultCh1, adcResultCh2(0~4095), adcVoltageCh1_mV, adcVoltageCh2_mV(0~3300), conversionCount
//
// 본 예제는 TI DriverLib API + FreeRTOS 정적 할당(configSUPPORT_DYNAMIC_ALLOCATION=0)만으로
// 제작되었습니다. 같은 동작을 비트필드로 구현한 9_VariableVoltage2Ch_Bitfield.c,
// 베어메탈 DriverLib으로 구현한 9_VariableVoltage2Ch_Driverlib.c와 함께 세 가지 방식을
// 비교해 보실 수 있습니다.
//
//************************************************************************************************************************************************************************


// 헤더 파일들
#include "driverlib.h"		// TI 제공 Driver API Library 헤더파일 (driverlib)
#include "device.h"
#include "FreeRTOS.h"
#include "task.h"

// 전처리 구문 정의
#define	ADC_ACQPS	75U		// SOC 샘플/홀드 창(윈도우) 크기
#define	STACK_SIZE	256U	// 태스크 스택 크기, 워드 단위 (C28x는 1워드=16비트이므로 512바이트)

// 함수 원형 선언
void initAdc(void);
void ADC_Task(void *pvParameters);

// 전역 변수 선언 (CCS Expressions/Watch 창에서 실시간으로 값을 확인할 수 있는 전역 변수)
volatile uint16_t	adcResultCh1 = 0U;		// A0(1채널) 12비트 변환 결과 (0~4095)
volatile uint16_t	adcResultCh2 = 0U;		// A1(2채널) 12비트 변환 결과 (0~4095)
volatile uint16_t	adcVoltageCh1_mV = 0U;	// 1채널을 mV로 환산한 값 (0~3300)
volatile uint16_t	adcVoltageCh2_mV = 0U;	// 2채널을 mV로 환산한 값 (0~3300)
volatile uint32_t	conversionCount = 0U;	// 변환 루프 실행 횟수

// FreeRTOS 태스크용 정적 메모리 선언 (힙 미사용, 정적 할당)
static StaticTask_t adcTaskBuffer;
static StackType_t  adcTaskStack[STACK_SIZE];
#pragma DATA_SECTION(adcTaskStack, ".freertosStaticStack")
#pragma DATA_ALIGN(adcTaskStack, portBYTE_ALIGNMENT)

static StaticTask_t idleTaskBuffer;
static StackType_t  idleTaskStack[STACK_SIZE];
#pragma DATA_SECTION(idleTaskStack, ".freertosStaticStack")
#pragma DATA_ALIGN(idleTaskStack, portBYTE_ALIGNMENT)


// 메인 함수
void main(void)
{

//	1. 전역 인터럽트 스위치 OFF, CPU 인터럽트 벡터 비-활성화 및 플래그(Flag) 비트 클리어
//	   (Interrupt_initModule() 함수가 한 번에 담당합니다)


//	2. 시스템 초기화 - Device_init( ) 함수 호출
//	* 왓치독 타이머 비-활성화
//	* CPU 클럭 주파수 설정 (PLL, 200MHz)
//	* 주변회로 클럭 공급 설정
	Device_init();

//	* 범용 입출력 포트(GPIO) 핀 락 해제, 내부 풀업 활성화 - Device_initGPIO( ) 함수 호출
	Device_initGPIO();


//	3. 주변회로 인터럽트 확장회로 초기화 - Interrupt_initModule( ) 함수 호출
	Interrupt_initModule();


//	4. 주변회로 인터럽트 벡터 확장 및 복사 실행 - Interrupt_initVectorTable( ) 함수 호출
	Interrupt_initVectorTable();


//	5. 인터럽트 벡터와 인터럽트 서비스 루틴 재-연결
//	   (본 예제는 인터럽트를 사용하지 않고 ADC_Task에서 ADCINT1 플래그를 직접 폴링합니다)


//	6. 주변회로 초기화 - ADC-A 2채널(ADCINA0/A1) 초기화
	initAdc();


//	7. 전역 변수 및 S/W 모듈 초기화
	conversionCount = 0U;


//	8. 실시간 디버깅 활성화, 전역 인터럽트 스위치 ON
	ERTM;	// Debug Enable Mask 비트 설정
	EINT;	// 전역 인터럽트 스위치 ON (/INTM ON)


//	9. FreeRTOS 태스크 생성 및 스케줄러 시작
//	   - ADC_Task (우선순위 1, 50ms 주기): ADCINA0/A1 변환 및 mV 환산
	xTaskCreateStatic(ADC_Task,				// 태스크 함수
	                  "ADC Task",			// 이름(디버깅용)
	                  STACK_SIZE,			// 스택 크기(워드)
	                  NULL,					// 파라미터 없음
	                  tskIDLE_PRIORITY + 1,	// 우선순위 1
	                  adcTaskStack,
	                  &adcTaskBuffer);

	vTaskStartScheduler();		// 이 아래로는 절대 돌아오지 않음

	for(;;)
	{
	    // 여기 도달하면 스케줄러 시작 실패 (메모리 부족 등)
	}
}

//	10. 인터럽트 서비스 루틴 및 기타 함수들

//
// ADC_Task - 50ms마다 SOC0(A0)/SOC1(A1)을 소프트웨어로 강제 트리거하고 변환 완료를
// 기다린 뒤, 12비트 결과와 mV 환산값을 전역 변수에 저장합니다.
//
void ADC_Task(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for(;;)
    {
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
        ADC_forceMultipleSOC(ADCA_BASE, (ADC_FORCE_SOC0 | ADC_FORCE_SOC1));

        uint16_t timeout = 0xFFFF;
        while((ADC_getInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1) == false) && (timeout > 0))
        {
            timeout--;
        }

        adcResultCh1 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
        adcResultCh2 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);

        adcVoltageCh1_mV = (uint16_t)(((uint32_t)adcResultCh1 * 3300U) / 4095U);
        adcVoltageCh2_mV = (uint16_t)(((uint32_t)adcResultCh2 * 3300U) / 4095U);

        conversionCount++;

        // vTaskDelayUntil을 사용하여 누적 오차 없이 정확히 50ms 간격으로 실행
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}

//
// initAdc - ADCA를 12bit 단일종단 모드로 초기화하고 ADCINA0/A1을 소프트웨어 트리거로 배정합니다.
//
void initAdc(void)
{
    // AIO227(A0, B Side 111번), AIO228(A1, B Side 109번) 아날로그 모드 활성화
    GPIO_setAnalogMode(227U, GPIO_ANALOG_ENABLED);
    GPIO_setAnalogMode(228U, GPIO_ANALOG_ENABLED);

    // 내부 3.3V 기준전압 활성화
    ASysCtl_setAnalogReferenceInternal(ASYSCTL_VREFHIA);
    ASysCtl_setAnalogReference1P65(ASYSCTL_VREFHIA);	// 3.3V / 1.65V 모드 (bit8=0)

    // ADC-A 클록 분주(50MHz, 200MHz SYSCLK 기준 /4) 및 12bit 단일종단 모드 설정
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);

    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ADCA_BASE);
    DEVICE_DELAY_US(5000U);	// ADC 코어 및 내부 VREF 안정화 대기시간 (5ms)

    // SOC 채널 배정 (소프트웨어 강제 트리거 모드)
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_SW_ONLY, ADC_CH_ADCIN0, ADC_ACQPS);
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_SW_ONLY, ADC_CH_ADCIN1, ADC_ACQPS);

    // SOC1(마지막 채널) 변환 완료 시 ADCINT1 플래그 발생 (단발성 모드)
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER1);
    ADC_disableContinuousMode(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

//
// vApplicationStackOverflowHook - FreeRTOS가 스택오버플로우를 감지하면 호출
//
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for(;;) { }
}

//
// vApplicationGetIdleTaskMemory - configSUPPORT_STATIC_ALLOCATION=1 이면 Idle 태스크
// 메모리도 애플리케이션이 직접 제공해야 함(힙을 안 쓰므로)
//
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &idleTaskBuffer;
    *ppxIdleTaskStackBuffer = idleTaskStack;
    *pulIdleTaskStackSize = STACK_SIZE;
}

// 파일 끝.
