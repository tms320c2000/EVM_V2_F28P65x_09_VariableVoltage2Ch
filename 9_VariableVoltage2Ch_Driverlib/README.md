# 가변 전압 출력 2채널 테스트 — DriverLib 버전 — TMS320F28P659DK8-Q1

TMS320F28P65x 개발보드 V2의 회로블록 **(9) 가변 전압 출력 2채널**을 순수 DriverLib
(SysConfig 미사용)로 읽습니다. 로터리 가변저항 2채널이 만드는 0~3.3V 아날로그 전압을
ADCA A0/A1 채널로 소프트웨어 트리거 방식으로 변환해 전역 변수에 저장합니다.

## 요구 하드웨어
- SyncWorks TMS320F28X 개발보드 V2
- TMS320F28P650DK9 또는 TMS320F28P659DK8-Q1 모듈
- 점퍼 케이블 2개

## 배선 (Bitfield, FreeRTOS 예제와 100% 동일)

개발보드 B Side 핀-헤더와 보드의 **(9) 가변 전압 출력 2채널** 블록을 점퍼 와이어로 연결하세요:

| 채널 | 가변 전압 출력 | ADC 입력 (B Side) |
|---|---|---|
| **1채널** | 출력1 (가변저항1) | ──▶ **B Side 111번** (ADCINA0) |
| **2채널** | 출력2 (가변저항2) | ──▶ **B Side 109번** (ADCINA1) |

> **안내**: F28P65x에서 A0/A1은 AIO227/AIO228로 디지털 GPIO와 멀티플렉싱되어 있어,
> `GPIO_setAnalogMode(pin, GPIO_ANALOG_ENABLED)`를 호출해 아날로그 서브시스템 스위치를
> 연결해야 신호가 정상 유입됩니다(전용 아날로그 핀이 아닙니다). 핀 매핑 근거:
> `SYNCWORKS_DEVBOARDV2_F28P65X.syscfg.json`.

![F28P65x 모듈 - 개발보드 V2 (9) 가변 전압 출력 2채널 배선도](f28xevm_v2_variable_voltage_2ch.png)

## 소프트웨어 구성
- CCS: 21.x (Theia 기반)
- **SysConfig 미사용** — `products="C2000WARE"`만 사용
- C2000Ware: 26.00.00.00 — `device.h`/`device.c`/`driverlib.h`/driverlib 헤더 전체/
  `driverlib.lib`를 이 폴더 안에 전부 복사해 두어 C2000Ware 설치 경로에 의존하지 않습니다.
- Code Generation Tools: 22.6.3.LTS

## 동작 방식

1. **GPIO 아날로그 모드 설정**: `GPIO_setAnalogMode(227/228, GPIO_ANALOG_ENABLED)`로
   AIO227/228의 디지털 입력 버퍼를 분리하고 아날로그 서브시스템 스위치를 연결합니다.
2. **ADC 초기화 (`initADC`)**:
   - `ASysCtl_setAnalogReferenceInternal()`로 내부 기준전압을 활성화하고,
     `ADC_setVREF(ADCA_BASE, ADC_REFERENCE_INTERNAL, ADC_REFERENCE_3_3V)`로 3.3V 모드를 설정합니다.
   - `ADC_setPrescaler()`/`ADC_setMode()`로 12bit 단일종단 모드를 구성하고 컨버터를 켭니다.
3. **SOC 채널 배정 (`initADCSOC`)**: SOC0→ADCINA0(1채널), SOC1→ADCINA1(2채널)을 모두
   소프트웨어 강제 트리거(SW Only)로 구성하고, SOC1 완료 시 `ADCINT1` 플래그가 서도록 합니다.
4. **메인 루프**: `ADC_forceMultipleSOC()`로 SOC0/1을 동시 트리거하고, `ADCINT1` 플래그를
   폴링한 뒤 두 채널의 12비트 결과를 읽어 mV로 환산합니다(50ms 주기).

## 정상 동작 확인

CCS Expressions 창에 아래 변수를 등록하고 Continuous Refresh를 켠 뒤, 보드의 가변전압
출력 블록 로터리 손잡이를 돌리면서 값이 매끄럽게 바뀌는지 확인하세요:

- `adcResultCh1`, `adcResultCh2` — 0~4095 범위의 원본 12비트 ADC 코드
- `adcVoltageCh1_mV`, `adcVoltageCh2_mV` — 0~3300 범위, mV 단위로 환산된 값
- `conversionCount` — 계속 증가하면 변환 루프가 정상적으로 돌고 있는 것입니다.

## Import → Build → Flash → Run
1. CCS에서 `CCS/9_VariableVoltage2Ch_Driverlib.projectspec`를 Import
2. Build (CPU1_RAM 또는 CPU1_FLASH)
3. Debug 연결 후 Flash/Run
4. `.ccxml`은 `TMS320F28P650DK9.ccxml`을 그대로 사용 — F28P659DK8-Q1 최초 연결 시 정상
   인식 확인 필요

## 세 가지 버전 비교
같은 가변 전압 출력 2채널 ADC 입력을 세 가지 방식으로 구현해 비교합니다.

| 버전 | 폴더 | 핵심 차이 |
|---|---|---|
| **DriverLib(이 폴더)** | 9_VariableVoltage2Ch_Driverlib | TI 표준 HAL 함수 호출 |
| 비트필드 | [9_VariableVoltage2Ch_Bitfield](../9_VariableVoltage2Ch_Bitfield/) | 레지스터 구조체 직접 조작, DriverLib 미사용 |
| FreeRTOS | [9_VariableVoltage2Ch_Freertos](../9_VariableVoltage2Ch_Freertos/) | DriverLib + 태스크 스케줄링, 정적 할당 |

### 메모리 실측 비교 (2026-09-18, CCS 21.x / C:\ti\ccs2100, CPU1_RAM 빌드, `.map` 기준)

세 프로젝트 모두 `C:\Users\vosam\workspace_ccstheia`에서 실제로 Import → Build까지 성공한
뒤의 `.map` 파일(TI 링커 Grand Total, 16비트 워드 단위)을 기준으로 한 실측치입니다 —
추정이 아닙니다.

| 버전 | code | ro data | rw data | 합계 |
|---|---:|---:|---:|---:|
| **DriverLib(이 폴더)** | 3,504 | 853 | 1,032 | **5,389** |
| 비트필드 | 3,756 | 474 | 2,837 | **7,067** |
| FreeRTOS | 5,556 | 1,080 | 1,241 | **7,877** |

- **비트필드의 rw data(2,837)가 DriverLib(1,032)보다 훨씬 큰 이유**는 클래식 헤더
  (`f28p65x_globalvariabledefs.obj`)가 이 예제와 무관한 페리페럴까지 포함한 전체
  레지스터 맵을 전역 선언하면서 rw data에 2,565워드를 잡아먹기 때문입니다 — 실제
  애플리케이션 자체가 쓰는 rw data는 `adcResultCh1/2`, `adcVoltageCh1/2_mV`,
  `conversionCount`를 합쳐 6워드뿐입니다.
- **비트필드의 code(3,756)가 DriverLib(3,504)보다 큰 이유**도 마찬가지로 클래식
  인터럽트 벡터 스텁(`f28p65x_defaultisr.obj`, 2,013워드)과 `InitSysCtrl()` 등
  초기화 헬퍼(862워드) 때문입니다 — 정작 애플리케이션 로직 자체는 비트필드가 162워드로
  DriverLib(583워드)보다 훨씬 작습니다.
- FreeRTOS는 DriverLib 대비 **code +2,052워드**(스케줄러·컨텍스트 스위칭 커널
  오버헤드)와 **rw data +209워드**(정적 태스크 스택 `adcTaskStack`/`idleTaskStack`
  각 256워드 등)가 순수 추가분입니다.

## 관련 링크
- 상품 페이지: https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903127
- 게시판 글: https://tms320f28x.co.kr/board/view.php?bdId=tms320f28xevmv2&sno=108
- 유튜브 영상: (게시 후 URL 추가 예정)
