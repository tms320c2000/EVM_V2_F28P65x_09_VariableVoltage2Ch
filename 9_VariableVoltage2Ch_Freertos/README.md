# 가변 전압 출력 2채널 테스트 — FreeRTOS 멀티태스킹 버전 — TMS320F28P659DK8-Q1

TMS320F28P65x 개발보드 V2의 회로블록 **(9) 가변 전압 출력 2채널**을 FreeRTOS 태스크
(정적 할당, 힙 미사용)와 DriverLib API로 읽습니다. 로터리 가변저항 2채널이 만드는
0~3.3V 아날로그 전압을 ADCA A0/A1 채널로 50ms마다 소프트웨어 트리거 방식으로 변환합니다.

## 이 버전의 핵심 — FreeRTOS 태스크 아키텍처

1. **태스크: `ADC_Task` (우선순위: `tskIDLE_PRIORITY + 1`, 주기: 50ms)**
   - `vTaskDelayUntil()`로 누적 오차 없이 정확히 50ms 간격으로 SOC0(A0)/SOC1(A1)을
     소프트웨어 강제 트리거합니다.
   - `ADCINT1` 플래그(타임아웃 보호 포함)를 확인한 뒤 12비트 결과를 읽어 mV로 환산합니다.
2. **완전한 정적 메모리 할당 (Zero Heap)**:
   - `configSUPPORT_DYNAMIC_ALLOCATION=0`, `configSUPPORT_STATIC_ALLOCATION=1` 설정으로
     힙 메모리를 일체 사용하지 않습니다.
   - 태스크 스택(`STACK_SIZE = 256`워드 = 512바이트)과 TCB, Idle 태스크 메모리까지 모두
     정적 전역 변수로 할당하여 `.freertosStaticStack` 섹션에 안전하게 배치했습니다.

## 요구 하드웨어 / 배선 (Bitfield, DriverLib 예제와 100% 동일)

개발보드 B Side 핀-헤더와 보드의 **(9) 가변 전압 출력 2채널** 블록을 점퍼 와이어로 연결하세요:

| 채널 | 가변 전압 출력 | ADC 입력 (B Side) |
|---|---|---|
| **1채널** | 출력1 (가변저항1) | ──▶ **B Side 111번** (ADCINA0) |
| **2채널** | 출력2 (가변저항2) | ──▶ **B Side 109번** (ADCINA1) |

> **안내**: F28P65x에서 A0/A1은 AIO227/AIO228로 디지털 GPIO와 멀티플렉싱되어 있어,
> `GPIO_setAnalogMode(pin, GPIO_ANALOG_ENABLED)`를 호출해 아날로그 서브시스템 스위치를
> 연결해야 신호가 정상 유입됩니다.

![F28P65x 모듈 - 개발보드 V2 (9) 가변 전압 출력 2채널 배선도](../9_VariableVoltage2Ch_Driverlib/f28xevm_v2_variable_voltage_2ch.png)

## 소프트웨어 구성
- CCS: 21.x (Theia 기반)
- **SysConfig 미사용** — `products="C2000WARE"`만 사용
- C2000Ware: 26.00.00.00 driverlib(로컬 복사)
- **FreeRTOS 커널 소스 전체를 `FreeRTOS/` 폴더에 로컬 복사** (TI kernel/FreeRTOS 배포본 기준)
- Code Generation Tools: 22.6.3.LTS

## Import → Build → Flash → Run
1. CCS에서 `CCS/9_VariableVoltage2Ch_Freertos.projectspec`를 Import
2. Build (CPU1_RAM 또는 CPU1_FLASH) — FreeRTOS 전용 링커 cmd(`28p65x_freertos_*_lnk_cpu1.cmd`) 사용.
3. Debug 연결 후 Flash/Run — `.ccxml`은 XDS2xx USB 디버그 프로브 설정 사용.

## 정상 동작 확인

CCS Expressions 창에 아래 변수를 등록하고 Continuous Refresh를 켠 뒤, 보드의 가변전압
출력 블록 로터리 손잡이를 돌리면서 값이 매끄럽게 바뀌는지 확인하세요:

- `adcResultCh1`, `adcResultCh2` — 0~4095 범위의 원본 12비트 ADC 코드
- `adcVoltageCh1_mV`, `adcVoltageCh2_mV` — 0~3300 범위, mV 단위로 환산된 값
- `conversionCount` — 계속 증가하면 `ADC_Task`가 정상적으로 돌고 있는 것입니다.

## 세 가지 버전 비교
같은 가변 전압 출력 2채널 ADC 입력을 세 가지 방식으로 구현해 비교합니다.

| 버전 | 폴더 | 핵심 차이 |
|---|---|---|
| DriverLib | [9_VariableVoltage2Ch_Driverlib](../9_VariableVoltage2Ch_Driverlib/) | TI 표준 HAL 함수 호출, 메인 루프 폴링 |
| 비트필드 | [9_VariableVoltage2Ch_Bitfield](../9_VariableVoltage2Ch_Bitfield/) | 레지스터 구조체 직접 조작, 메인 루프 폴링 |
| **FreeRTOS(이 폴더)** | 9_VariableVoltage2Ch_Freertos | DriverLib + FreeRTOS 태스크 스케줄링(정적 할당, ADC_Task) |

### 메모리 실측 비교 (2026-09-18, CCS 21.x / C:\ti\ccs2100, CPU1_RAM 빌드, `.map` 기준)

세 프로젝트 모두 `C:\Users\vosam\workspace_ccstheia`에서 실제로 Import → Build까지 성공한
뒤의 `.map` 파일(TI 링커 Grand Total, 16비트 워드 단위)을 기준으로 한 실측치입니다 —
추정이 아닙니다.

| 버전 | code | ro data | rw data | 합계 |
|---|---:|---:|---:|---:|
| DriverLib | 3,504 | 853 | 1,032 | **5,389** |
| 비트필드 | 3,756 | 474 | 2,837 | **7,067** |
| **FreeRTOS(이 폴더)** | 5,556 | 1,080 | 1,241 | **7,877** |

- **이 FreeRTOS 버전은 DriverLib 대비 code +2,052워드**가 순수 커널 오버헤드(스케줄러,
  컨텍스트 스위칭)이고, rw data는 정적 태스크 스택 2개(`adcTaskStack`/`idleTaskStack`,
  각 `STACK_SIZE=256`워드)가 추가되어 +209워드 순증가합니다.
- 비트필드가 rw data(2,837)만 유독 큰 것은 이 FreeRTOS 버전과 무관한 클래식 헤더
  전역 레지스터 구조체 때문이며, 자세한 원인은
  [9_VariableVoltage2Ch_Bitfield/README.md](../9_VariableVoltage2Ch_Bitfield/README.md#메모리-실측-비교-2026-09-18-ccs-21x--cticcs2100-cpu1_ram-빌드-map-기준)를
  참고하세요.

## 관련 링크
- 상품 페이지: https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903127
- 게시판 글: (게시 후 URL 추가 예정)
- 유튜브 영상: (게시 후 URL 추가 예정)
