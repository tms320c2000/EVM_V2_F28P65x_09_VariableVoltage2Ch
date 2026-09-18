// 파일이름	:	9_VariableVoltage2Ch_Driverlib.c
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
// a. 배선 안내:
//
//		>> 1채널: 가변전압 출력 1채널 ──▶ B Side 111번 핀 (ADCINA0)
//		>> 2채널: 가변전압 출력 2채널 ──▶ B Side 109번 핀 (ADCINA1)
//
//	 (Bitfield 예제, FreeRTOS 예제와 완전히 동일한 점퍼 배선으로 공용할 수 있습니다.)
//
// 예제 폴더의 CCS Expressions 창에 아래 변수를 등록해두면 동작을 실시간으로 확인할 수 있습니다.
// --> adcResultCh1, adcResultCh2(0~4095), adcVoltageCh1_mV, adcVoltageCh2_mV(0~3300), conversionCount
//
// 본 예제는 TI가 제공하는 Driver API Library(DriverLib) 코드만으로 제작되었습니다
// (SysConfig 미사용). 같은 동작을 레지스터를 직접 조작하는 Bit-Field 방식으로 구현한
// 9_VariableVoltage2Ch_Bitfield.c, FreeRTOS 멀티태스킹으로 구현한
// 9_VariableVoltage2Ch_Freertos.c와 함께 세 가지 방식을 비교해 보실 수 있습니다.
//
// b. F28P65x에서 A0/A1은 AIO227/AIO228로 디지털 GPIO와 멀티플렉싱되어 있어, 반드시
//    GPIO_setAnalogMode(..., GPIO_ANALOG_ENABLED)를 설정해야 ADC 입력 스위치가 연결됩니다.
//    (핀 매핑 근거: dev_board_v2/10-sysconfig-보드파일/SYNCWORKS_DEVBOARDV2_F28P65X.syscfg.json)
//
//************************************************************************************************************************************************************************

#include "driverlib.h"
#include "device.h"

//
// CCS Expressions/Watch 창에서 실시간으로 값을 확인할 수 있는 전역 변수
//
volatile uint16_t adcResultCh1 = 0U;
volatile uint16_t adcResultCh2 = 0U;
volatile uint16_t adcVoltageCh1_mV = 0U;
volatile uint16_t adcVoltageCh2_mV = 0U;
volatile uint32_t conversionCount = 0U;

void initADC(void);
void initADCSOC(void);

//
// Main
//
void main(void)
{
    Device_init();
    Device_initGPIO();
    Interrupt_initModule();
    Interrupt_initVectorTable();

    //
    // A0(AIO227, B-Side 111번), A1(AIO228, B-Side 109번) 아날로그 모드 활성화
    // F28P65x는 전용 핀이 아니므로 반드시 아날로그 모드로 지정해야 ADC 입력 스위치가 연결됩니다.
    //
    GPIO_setPinConfig(GPIO_227_GPIO227);
    GPIO_setAnalogMode(227, GPIO_ANALOG_ENABLED);

    GPIO_setPinConfig(GPIO_228_GPIO228);
    GPIO_setAnalogMode(228, GPIO_ANALOG_ENABLED);

    initADC();
    initADCSOC();

    for(;;)
    {
        //
        // SOC0(A0), SOC1(A1) 동시 소프트웨어 트리거
        //
        ADC_forceMultipleSOC(ADCA_BASE, (ADC_FORCE_SOC0 | ADC_FORCE_SOC1));

        //
        // 변환 완료(ADCINT1) 대기
        //
        while(ADC_getInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1) == false)
        {
        }
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);

        //
        // 결과 저장 및 mV 환산 (12비트, VREF 3.3V 기준)
        //
        adcResultCh1 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
        adcResultCh2 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);

        adcVoltageCh1_mV = (uint16_t)(((uint32_t)adcResultCh1 * 3300U) / 4095U);
        adcVoltageCh2_mV = (uint16_t)(((uint32_t)adcResultCh2 * 3300U) / 4095U);

        conversionCount++;

        DEVICE_DELAY_US(50000U);
    }
}

//
// initADC - ADCA 모듈 기본 설정 (12비트, 싱글엔디드, 내부 파워업)
//
void initADC(void)
{
    //
    // 내부 기준전압(VREF) 활성화: ASysCtl에서 내부 1.65V/3.3V 기준전압 버퍼 공급
    //
    ASysCtl_setAnalogReferenceInternal(ASYSCTL_VREFHIA | ASYSCTL_VREFHIB | ASYSCTL_VREFHIC);
    ASysCtl_setAnalogReference1P65(ASYSCTL_VREFHIA | ASYSCTL_VREFHIB | ASYSCTL_VREFHIC);

    //
    // ADC 기준전압 모드 및 오프셋 트림 설정
    //
    ADC_setVREF(ADCA_BASE, ADC_REFERENCE_INTERNAL, ADC_REFERENCE_3_3V);

    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ADCA_BASE);

    //
    // ADC 코어 및 내부 VREF 밴드갭 안정화를 위해 5ms 대기
    //
    DEVICE_DELAY_US(5000U);
}

//
// initADCSOC - SOC0=A0채널, SOC1=A1채널을 소프트웨어 트리거로 구성
//
void initADCSOC(void)
{
    //
    // SOC0: A0 채널, 샘플 윈도우 75 SYSCLK 사이클
    //
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_SW_ONLY,
                 ADC_CH_ADCIN0, 75U);

    //
    // SOC1: A1 채널, 샘플 윈도우 75 SYSCLK 사이클
    //
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_SW_ONLY,
                 ADC_CH_ADCIN1, 75U);

    //
    // SOC1 완료 시 ADCINT1 인터럽트 플래그 발생 (SOC0/SOC1 둘 다 끝난 뒤 시점)
    //
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_INT_TRIGGER_EOC1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_disableContinuousMode(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
}

//
// End of File
//
