# cms-string

**High-performance, Zero-Heap, UTF-8 Safe String Library for Embedded Systems.**

`cms-string`은 메모리 자원이 제한된 임베디드 환경(ESP32, Arduino 등)에서 힙 파편화(Heap Fragmentation) 걱정 없이 안전하고 빠르게 문자열을 다루기 위해 설계된 C++ 라이브러리입니다.

## ✨ 주요 특징

- **Zero-Heap Policy**: 모든 문자열은 정적 배열(`String<N>`)에 저장됩니다. 런타임 중 `malloc`이나 `new`를 전혀 사용하지 않아 시스템 안정성이 극대화됩니다.
- **UTF-8 Awareness**: 단순 바이트 단위 처리가 아닌, 논리적 글자 단위의 인덱싱, 검색, 자르기를 지원하여 한글 등 멀티바이트 문자가 깨지는 것을 방지합니다.
- **Non-destructive Splitting**: 원본 문자열을 수정하지 않고 포인터와 길이 정보만으로 문자열을 분리하는 `Token` 시스템을 제공합니다.
- **Lightweight Formatting**: 표준 `vsnprintf` 대비 스택 소모가 적은 초경량 포맷팅 엔진(`appendPrintf`)을 내장하고 있습니다.
- **Built-in Profiling**: 버퍼 사용률(`utilization`)과 피크치(`peakUtilization`)를 실시간으로 모니터링하여 적절한 버퍼 크기 설정을 돕습니다.

## 📦 설치 방법 (PlatformIO)

`platformio.ini` 파일의 `lib_deps` 항목에 아래와 같이 추가하세요.

```ini
lib_deps =
    https://github.com/your-username/cms-string.git
```

## 🚀 빠른 시작

### 1. 선언 및 기본 사용

```cpp
#include <cmsString.h>

// 64바이트 고정 크기 버퍼를 가진 문자열 선언
cms::String<64> str = "Hello";

// 스트림 스타일 결합
str << " World! " << 2024 << " [OK]";

Serial.println(str.c_str()); // "Hello World! 2024 [OK]"
```

### 2. UTF-8 안전한 조작

```cpp
cms::String<64> ko = "안녕하세요";

// 논리적 글자 수 반환 (바이트 수가 아님)
size_t len = ko.count(); // 5

// 글자 단위 부분 문자열 추출
cms::String<32> sub;
ko.substring(sub, 0, 2); // "안녕"
```

### 3. 비파괴적 문자열 분리 (Token)

```cpp
cms::String<64> data = "SENSOR:25.4:80";
cms::string::Token tokens[3];

// 원본 data를 수정하지 않고 분리
size_t count = data.split(':', tokens, 3);

if (count >= 2) {
    if (tokens[0] == "SENSOR") {
        double val = tokens[1].toFloat(); // 25.4
    }
}
```

### 4. 리터럴 최적화

문자열 리터럴을 사용할 경우 컴파일 타임에 길이를 계산하여 런타임 `strlen` 오버헤드를 제거합니다.

```cpp
if (str == "COMMAND") { ... } // 고속 비교
str << "Data";               // 고속 결합
```

## 📊 성능 모니터링

임베디드 시스템의 리소스 최적화를 위해 현재 버퍼 상태를 확인할 수 있습니다.

```cpp
float current = str.utilization();     // 현재 사용률 (%)
float peak = str.peakUtilization();    // 객체 생성 후 최대 도달 사용률 (%)
```

## 🛠 빌드 설정 권장사항

한글 깨짐 방지 및 최신 C++ 기능을 위해 `platformio.ini`에 아래 설정을 추가하는 것을 권장합니다.

```ini
build_flags =
    -std=gnu++17
    -finput-charset=UTF-8
    -fexec-charset=UTF-8
```

## 📄 라이선스

이 프로젝트는 MIT 라이선스 하에 배포됩니다.

---
**Maintainer:** comser.dev
**Repository:** github.com/your-username/cms-string
