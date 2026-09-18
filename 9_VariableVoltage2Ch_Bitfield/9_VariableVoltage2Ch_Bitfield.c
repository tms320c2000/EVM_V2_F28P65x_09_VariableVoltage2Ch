// 파일이름	:	9_VariableVoltage2Ch_Bitfield.c
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
//	 (DriverLib 예제, FreeRTOS 예제와 완전히 동일한 점퍼 배선으로 공용할 수 있습니다.)
//
// 예제 폴더의 CCS Expressions 창에 아래 변수를 등록해두면 동작을 실시간으로 확인할 수 있습니다.
// --> adcResultCh1, adcResultCh2(0~4095), adcVoltageCh1_mV, adcVoltageCh2_mV(0~3300), conversionCount
//
// 본 예제는 28X 칩 MMR(Memory Mapped Register)을 직접 조작하는 TI의 Bit-Field Approach
// 만으로 제작되었습니다 (Driverlib 미사용, driverlib.lib 자체가 이 프로젝트에는 없습니다).
// 같은 동작을 DriverLib API로 구현한 9_VariableVoltage2Ch_Driverlib.c, FreeRTOS 태스크로
// 구현한 9_VariableVoltage2Ch_Freertos.c와 함께 세 가지 방식을 비교해 보실 수 있습니다.
//
// b. F28P65x에서 A0/A1은 AIO227/AIO228로 디지털 GPIO와 멀티플렉싱되어 있어, 반드시
//    GPHAMSEL/AGPIOCTRLH를 아날로그 모드로 설정해야 ADC 입력 스위치가 연결됩니다.
//
//************************************************************************************************************************************************************************


// 헤더 파일들
#include "f28x_project.h"		// TI 제공 칩-지원 헤더 통합 Include 용 헤더파일 (bit-field)

// 전처리 구문 정의
#define	ADC_ACQPS	75U		// SOC 샘플/홀드 창(윈도우) 크기 - DriverLib 버전과 동일 값

// 함수 원형 선언
void initADC(void);
void initADCSOC(void);

// 전역 변수 선언 (CCS Expressions/Watch 창에서 실시간으로 값을 확인할 수 있는 전역 변수)
volatile Uint16	adcResultCh1;			// A0(1채널) 12비트 변환 결과 (0~4095)
volatile Uint16	adcResultCh2;			// A1(2채널) 12비트 변환 결과 (0~4095)
volatile Uint16	adcVoltageCh1_mV;		// 1채널을 mV로 환산한 값 (0~3300)
volatile Uint16	adcVoltageCh2_mV;		// 2채널을 mV로 환산한 값 (0~3300)
volatile Uint32	conversionCount;		// 변환 루프 실행 횟수


// 메인 함수
void main(void)
{

//	1. 전역 인터럽트 스위치 OFF, CPU 인터럽트 벡터 비-활성화 및 플래그(Flag) 비트 클리어
	DINT;			// 전역 인터럽트 스위치 OFF (/INTM OFF)
	IER = 0x0000;	// CPU 인터럽트 벡터 비-활성화
	IFR = 0x0000;	// CPU 인터럽트 플래그 클리어


//	2. 시스템 초기화 - InitSysCtrl( ) 함수 호출 (f28p65x_sysctrl.c)
//	* 왓치독 타이머 비-활성화
//	* CPU 클럭 주파수 설정 (PLL)
//	* 주변회로 클럭 공급 설정
	InitSysCtrl();

//	* 범용 입출력 포트(GPIO) 설정 - InitGpio( ) 함수 호출
	InitGpio();

	//
	// A0(AIO227, B Side 111번), A1(AIO228, B Side 109번) 아날로그 모드 활성화
	// F28P65x는 전용 아날로그 핀이 아니므로 반드시 GPIO 뮤텍스를 GPIO 기능으로 두고
	// GPHAMSEL/AGPIOCTRLH를 아날로그 모드로 설정해야 ADC 입력 스위치가 연결됩니다.
	//
	EALLOW;
	GpioCtrlRegs.GPHMUX1.bit.GPIO227 = 0;			// GPIO227 -> GPIO 기능(Mux=0)
	GpioCtrlRegs.GPHMUX1.bit.GPIO228 = 0;			// GPIO228 -> GPIO 기능(Mux=0)
	GpioCtrlRegs.GPHAMSEL.bit.GPIO227 = 1;			// GPIO227 아날로그 모드 활성화
	GpioCtrlRegs.GPHAMSEL.bit.GPIO228 = 1;			// GPIO228 아날로그 모드 활성화
	AnalogSubsysRegs.AGPIOCTRLH.bit.GPIO227 = 1;	// 아날로그 서브시스템 스위치 연결
	AnalogSubsysRegs.AGPIOCTRLH.bit.GPIO228 = 1;
	EDIS;


//	3. 주변회로 인터럽트 확장회로 초기화 - InitPieCtrl( ) 함수 호출 (f28p65x_piectrl.c)
	InitPieCtrl();


//	4. 주변회로 인터럽트 벡터 확장 및 복사 실행 - InitPieVectTable( ) 함수 호출 (f28p65x_pievect.c)
	InitPieVectTable();


//	5. 인터럽트 벡터와 인터럽트 서비스 루틴 재-연결
//	   (본 예제는 인터럽트를 사용하지 않고 메인 루프에서 ADCINT1 플래그를 직접 폴링합니다)


//	6. 주변회로 초기화 - ADC-A 2채널(ADCINA0/A1) 초기화
	initADC();
	initADCSOC();


//	7. 전역 변수 및 S/W 모듈 초기화
	adcResultCh1 = 0U;
	adcResultCh2 = 0U;
	adcVoltageCh1_mV = 0U;
	adcVoltageCh2_mV = 0U;
	conversionCount = 0UL;


//	8. 실시간 디버깅 활성화, 전역 인터럽트 스위치 ON
	ERTM;	// Debug Enable Mask 비트 설정 (실시간 디버깅이 가능하도록 ST1 레지스터의 /DBGM 비트를 0으로 클리어)
	EINT;	// 전역 인터럽트 스위치 ON (/INTM ON)


//	9. Idle(Background) Loop - SOC0/1 소프트웨어 강제 트리거 후 폴링
	for(;;)
	{
		//
		// SOC0(A0), SOC1(A1) 동시 소프트웨어 트리거
		//
		AdcaRegs.ADCSOCFRC1.all = 0x0003;		// bit0=SOC0, bit1=SOC1 동시 강제 트리거

		//
		// 변환 완료(ADCINT1) 대기
		//
		while(AdcaRegs.ADCINTFLG.bit.ADCINT1 == 0)
		{
		}
		AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;	// 플래그 클리어

		//
		// 결과 저장 및 mV 환산 (12비트, VREF 3.3V 기준)
		//
		adcResultCh1 = AdcaResultRegs.ADCRESULT0;
		adcResultCh2 = AdcaResultRegs.ADCRESULT1;

		adcVoltageCh1_mV = (Uint16)(((Uint32)adcResultCh1 * 3300UL) / 4095UL);
		adcVoltageCh2_mV = (Uint16)(((Uint32)adcResultCh2 * 3300UL) / 4095UL);

		conversionCount++;

		DELAY_US(50000);	// 50msec 지연
	}
}

//	10. 기타 함수들 (본 예제는 별도 인터럽트 서비스 루틴이 없습니다)

//
// initADC - ADCA 모듈 기본 설정 (12비트, 싱글엔디드, 내부 VREF 3.3V, OTP trim 로드)
//
void initADC(void)
{
	EALLOW;

	//
	// 내부 기준전압(VREF) 활성화: ANAREFCTL에서 내부 1.65V/3.3V 기준전압 버퍼 공급
	//
	AnalogSubsysRegs.ANAREFCTL.bit.ANAREFASEL = 0;		// 0: 내부 VREF 선택
	AnalogSubsysRegs.ANAREFCTL.bit.ANAREFA2P5SEL = 0;	// 0: 3.3V / 1.65V 모드

	AdcaRegs.ADCCTL2.bit.PRESCALE = 6;		// ADCCLK = SYSCLK/4 (50MHz, 200MHz SYSCLK 기준 권장값)

	//
	// RESOLUTION/SIGNALMODE를 레지스터에 직접 쓰는 대신 반드시 AdcSetMode()를
	// 통해서 설정해야 합니다 - AdcSetMode()는 그 두 비트만 쓰는 게 아니라 CalAdcINL()로
	// 공장 출하 시 OTP에 저장된 선형성(INL) trim 값을 로드하고 ADCCTL2.bit.OFFTRIMMODE를
	// "개별 채널별 오프셋 trim 사용"으로 설정합니다. 이 trim이 없으면 실제 입력 전압과
	// 무관하게 변환 결과가 풀스케일(4095) 근처로 고정될 수 있습니다.
	//
	AdcSetMode(ADC_ADCA, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);

	AdcaRegs.ADCCTL1.bit.INTPULSEPOS = 1;	// 변환 결과가 레지스터에 반영된 뒤 인터럽트/플래그 발생
	AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1;		// ADC 아날로그 회로 파워-업
	DELAY_US(5000);							// ADC 코어 및 내부 VREF 밴드갭 안정화 대기시간 (5ms)

	EDIS;
}

//
// initADCSOC - SOC0=A0채널, SOC1=A1채널을 소프트웨어 트리거로 구성
//
void initADCSOC(void)
{
	EALLOW;

	//
	// SOC0: A0 채널, 샘플 윈도우 75 SYSCLK 사이클
	//
	AdcaRegs.ADCSOC0CTL.bit.CHSEL = 0;		// SOC0 -> ADCINA0 (1채널, B Side 111번 핀)
	AdcaRegs.ADCSOC0CTL.bit.ACQPS = ADC_ACQPS;
	AdcaRegs.ADCSOC0CTL.bit.TRIGSEL = 0;	// 0 = 소프트웨어 강제 트리거 전용(SW Only)

	//
	// SOC1: A1 채널, 샘플 윈도우 75 SYSCLK 사이클
	//
	AdcaRegs.ADCSOC1CTL.bit.CHSEL = 1;		// SOC1 -> ADCINA1 (2채널, B Side 109번 핀)
	AdcaRegs.ADCSOC1CTL.bit.ACQPS = ADC_ACQPS;
	AdcaRegs.ADCSOC1CTL.bit.TRIGSEL = 0;

	//
	// SOC1 완료 시 ADCINT1 인터럽트 플래그 발생 (SOC0/SOC1 둘 다 끝난 뒤 시점)
	//
	AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 1;		// ADCINT1 = SOC1(마지막 채널) 변환 완료 시점에 발생
	AdcaRegs.ADCINTSEL1N2.bit.INT1CONT = 0;	// 연속 모드 비활성화
	AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1;		// ADCINT1 플래그 생성 활성화
	AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;		// 시작 전 플래그 클리어

	EDIS;
}

//
// End of File
//
