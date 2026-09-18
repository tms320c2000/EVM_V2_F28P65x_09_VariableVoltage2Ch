# TMS320F28X 개발보드 V2 — F28P65x 가변 전압 출력 2채널 예제

[TMS320F28X 범용 개발보드 V2](https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903127)의
회로블록 **(9) 가변 전압 출력 2채널**을 F28P65x 모듈로 읽는 예제입니다. 로터리 가변저항
2채널이 만드는 0~3.3V 아날로그 전압을 ADCA A0/A1 채널로 소프트웨어 트리거 방식으로 변환해
12비트 코드와 mV 값을 전역 변수에 저장합니다. 같은 동작을 세 가지 방식으로 각각 구현해서
코드량·메모리 사용량을 비교할 수 있게 만들었습니다.

## 포함된 프로젝트

| 프로젝트 | 설명 |
|---|---|
| [9_VariableVoltage2Ch_Bitfield](9_VariableVoltage2Ch_Bitfield/) | 레지스터 비트필드 직접 제어 (DriverLib 미사용) |
| [9_VariableVoltage2Ch_Driverlib](9_VariableVoltage2Ch_Driverlib/) | TI DriverLib 사용 |
| [9_VariableVoltage2Ch_Freertos](9_VariableVoltage2Ch_Freertos/) | FreeRTOS 태스크로 구현 (정적 할당, 힙 미사용) |

각 폴더는 자기완결형(self-contained) CCS 프로젝트입니다 — 폴더 하나만 받아도 C2000Ware/
DriverLib 등 필요한 파일이 전부 로컬에 포함되어 있어 Import → Build가 됩니다. OTP trim 로드
(`AdcSetMode()`), AIO227/228 아날로그 스위치 설정까지 각 폴더의 README.md "동작 방식" 절에서
자세히 설명합니다. 실측 메모리 비교도 각 폴더의 README.md를 참고하세요.

## 배선

개발보드 B Side 핀-헤더와 보드의 **(9) 가변 전압 출력 2채널** 블록을 점퍼 와이어로 연결하세요
(세 프로젝트 모두 동일한 배선을 공용합니다):

| 채널 | 가변 전압 출력 | ADC 입력 (B Side) |
|---|---|---|
| **1채널** | 출력1 (가변저항1) | ──▶ **B Side 111번** (ADCINA0) |
| **2채널** | 출력2 (가변저항2) | ──▶ **B Side 109번** (ADCINA1) |

![F28P65x 모듈 - 개발보드 V2 (9) 가변 전압 출력 2채널 배선도](9_VariableVoltage2Ch_Driverlib/f28xevm_v2_variable_voltage_2ch.png)

## 프로세서 모듈

- [TMS320F28P650DK9 모듈(산업용)](https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903200)
- [TMS320F28P659DK8-Q1 모듈(차량 전장용)](https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903201)

## 개발 환경

CCS 21.x(Theia 기반) / TI CGT 22.6.3.LTS / C2000Ware 26.00.00.00

## Import 방법 (zip 직접 import도 지원)

압축을 미리 풀어서 "Select search-directory"로 지정하거나, GitHub에서 받은 zip 파일을
그대로 CCS의 "Select archive file"로 지정해도 됩니다 — 둘 다 정상 동작합니다
(`driverlib.lib`가 zip-import 시 unresolved로 뜨던 버그를 고쳤습니다).
