# AI 스마트팜

[English](README.md) | **한국어**

<p align="center"><img src="images/system-concept.png" alt="개념도: 안드로이드 앱, 제어 보드, 스마트 재배 돔" width="100%"></p>
<p align="center"><sub>개념 일러스트</sub></p>

지니에듀테크(Jinie EduTech)의 교실용 실내 스마트팜 키트입니다. MIT App Inventor로 만든
안드로이드 앱에서 센서 값을 확인하고, 생장등·팬·펌프를 켜고 끄며, ESP32-CAM 실시간
카메라를 볼 수 있습니다. 머신러닝 실습은 앱 기능이 아니라 PC/Colab에서 하는 별도 수업입니다.

## 구성

| 구성 요소 | 역할 | 폴더 |
|------|--------------|--------|
| 컨트롤러 | 아두이노 나노가 센서를 읽어 LCD에 표시하고 생장등·팬·펌프를 제어 | [`firmware/nano-controller`](firmware/nano-controller) |
| 카메라 | ESP32-CAM이 Wi-Fi로 실시간 영상과 사진을 제공 | [`firmware/esp32-cam`](firmware/esp32-cam) |
| 안드로이드 앱 | MIT App Inventor 앱: 센서 데이터, 수동 제어, 카메라 | [`android`](android) |
| 수업 자료 | AI 스마트팜 활용 데이터와 기계학습 (파워포인트, 한국어) | [`docs`](docs) |
| 식물 상태 AI | YOLOv8 / Keras 상추 분류 모델 (별도 저장소) | [lettuce_ML](https://github.com/incom8134-web/lettuce_ML) |

## 앱 다운로드

- [안드로이드 APK (V11)](android/Ai_Smart_Farm_V11.apk) – 휴대폰에 설치합니다.
- [MIT App Inventor 프로젝트 (V11)](android/Ai_Smart_Farm_V11.aia) – 편집 가능한 소스입니다.
- [설치 및 테스트 안내](android/README.ko.md)

수업 자료: [파워포인트](<docs/AI 스마트팜 활용하는 데이터 와 기계학습_최종버전_v3_2026.10.14.pptx>)

## 시스템 구조

<p align="center"><img src="images/architecture.svg" alt="시스템 구조: 안드로이드 앱, 아두이노 나노, ESP32-CAM, 스마트 재배 돔, 생육 판별 모델" width="100%"></p>

## 기능

- 온도, 습도, 조도, 수위 실시간 표시
- 생장등, 팬, 워터 펌프 원격 켜기/끄기
- 앱에서 실시간 카메라 보기
- 카메라 이미지 머신러닝을 위한 별도 PC/Colab 실습
- 앱 언어 및 테마 설정

## 하드웨어

| 부품 | 비고 |
|-----------|-------|
| 아두이노 나노 | 메인 컨트롤러 |
| HC-06 | 블루투스 클래식 모듈 |
| DHT11 | 온도 및 습도 |
| 조도 센서, 수위 센서 | 아날로그 입력 |
| 16x2 I2C LCD | 주소 `0x27` (I2C 스캔 결과가 `0x3F`이면 그 값 사용) |
| LED 생장등, 팬, 워터 펌프 | 스위칭 출력 |
| AI Thinker ESP32-CAM | 카메라 |

### 나노 핀 배치

| 기능 | 핀 |
|----------|-----|
| LED / 생장등 | D4 |
| DHT11 | D5 |
| 팬 | D6 |
| 펌프 | D7 |
| HC-06 TX → 나노 RX | D8 |
| HC-06 RX ← 나노 TX | D9 |
| 조도 센서 | A0 |
| 수위 센서 | A1 |

## 설치

보드 설정, 라이브러리, 배선, 통신 규약, 전체 업로드 절차는 [펌웨어 안내](firmware/README.ko.md)에 있습니다. 요약하면:

1. `firmware/esp32-cam/esp32-cam.ino`를 **먼저** 업로드합니다. 폴더 전체를 함께 두세요.
   교실 2.4 GHz 공유기에 맞게 `WIFI_SSID`와 `WIFI_PASSWORD`를 입력합니다.
   보드는 AI Thinker ESP32-CAM, ESP32 코어 3.3.2, Core Debug Level **None**을 선택합니다.
2. ESP32를 리셋하기 전에 시리얼 모니터를 115200으로 열고, 한 번 출력되는 카메라 IP를 적어 둡니다.
3. 전원을 끄고 보드를 나노 프로그래밍 경로로 바꾼 뒤
   `firmware/nano-controller/nano-controller.ino`를 **두 번째로** 업로드합니다.
4. 전원을 끄고 일반 동작 모드를 선택한 뒤, 안드로이드 설정에서 HC-06을 페어링하고
   출력을 하나씩 시험합니다.

| URL | 용도 |
|-----|---------|
| `http://<카메라-IP>/` | 카메라 보기 페이지 |
| `http://<카메라-IP>:81/stream` | 실시간 MJPEG 스트림 |
| `http://<카메라-IP>/capture` | JPEG 사진 한 장 |
| `http://<카메라-IP>/status` | 카메라 형식 |

휴대폰과 카메라는 같은 공유기에 연결되어 있어야 합니다. 시작할 때 출력된 카메라 IP를 앱에 입력하세요.

## 블루투스 통신 규약

앱이 보내는 명령 (일반 텍스트, 끝에 `;` 추가):

`LED_ON`, `LED_OFF`, `FAN_ON`, `FAN_OFF`, `PUMP_ON`, `PUMP_OFF`

나노가 2초마다 보내는 센서 데이터:

```
온도,습도,조도원시값,수위원시값;
```

## 안전

나노는 수동 모드로 동작합니다. 센서는 값만 보고하고, 출력은 앱이 명령을 보낼 때만 바뀝니다.
저수위나 연결 끊김 시 자동으로 꺼지는 기능은 없으며, 앱을 닫아도 동작 중인 펌프는 멈추지 않습니다.
OFF 명령을 보내고 펌프가 실제로 멈췄는지 확인하세요. 펌프를 물 없이 돌리지 마세요.
DIP 스위치를 바꾸기 전에는 USB와 외부 전원을 모두 분리하세요.

## 저장소 구성

```
.
├── android/            안드로이드 앱 (.apk)과 MIT App Inventor 소스 (.aia)
├── firmware/
│   ├── nano-controller/  아두이노 나노 펌웨어
│   ├── esp32-cam/        ESP32-CAM 펌웨어
│   ├── sensor-tests/     부품을 하나씩 시험하는 간단한 스케치
│   ├── serial-output-test/  시리얼 모니터로 출력을 켜고 끄는 벤치 시험
│   └── check_firmware.py    펌웨어 오프라인 검사 (개발자용)
├── images/             README에 쓰인 그림
└── docs/               수업 발표 자료 (파워포인트)
```
