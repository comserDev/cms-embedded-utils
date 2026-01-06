/// @author comser.dev
///
/// 문자열 라이브러리의 공통 비즈니스 로직을 담당하는 베이스 클래스 정의서입니다.
/// 실제 데이터 버퍼를 소유하지 않고 주입된 메모리를 관리함으로써 템플릿 코드 비대화(Code Bloat)를
/// 방지하고 시스템 자원을 효율적으로 사용합니다.

#ifndef CMS_STRING_BASE_H
#define CMS_STRING_BASE_H
#include <stddef.h> // size_t 정의
#include <stdarg.h> // va_list 정의
#include <cstring>  // strlen, strcpy 등 표준 함수

namespace cms {

    namespace string {
        struct Token;
    }

// ==================================================================================================
// [StringBase] 개요
// - 왜 존재하는가: 다양한 크기의 String 템플릿 객체들이 공통 로직을 공유하여 바이너리 크기를 줄이기 위해 존재합니다.
// - 어떻게 동작하는가: 외부 버퍼 포인터와 용량 정보를 유지하며, 모든 가공은 cms::string 유틸리티를 통해 수행합니다.
// ==================================================================================================

    /// 모든 cms::String 객체의 공통 인터페이스와 로직을 정의하는 추상화 계층입니다.
    ///
    /// Why: 코드 중복을 제거하고 문자열 가공 로직을 중앙 집중화하기 위함입니다.
    /// How: 외부에서 주입된 버퍼 포인터를 기반으로 인플레이스(In-place) 가공을 수행합니다.
    ///
    /// @note 직접 인스턴스화할 수 없으며, 반드시 String<N> 자식 클래스를 통해 사용해야 합니다.
    class StringBase {
    public:
        virtual ~StringBase() = default;

        /// 현재 버퍼의 사용량을 퍼센트(%) 단위로 계산합니다.
        ///
        /// Why: 런타임 중 버퍼 오버플로우 위험을 모니터링하기 위함입니다.
        /// How: 현재 문자열 길이를 가용 용량으로 나누어 계산합니다.
        ///
        /// 사용 예:
        /// @code
        /// float usage = s.utilization();
        /// @endcode
        ///
        /// @return 0.0 ~ 100.0 사이의 현재 사용률
        float utilization() const;

        /// 객체 생성 이후 도달했던 최대 버퍼 사용량을 퍼센트(%) 단위로 반환합니다.
        ///
        /// Why: 시스템 운영 중 버퍼 부족 위험이 있었는지 판단하는 프로파일링 지표로 활용됩니다.
        /// How: 데이터 변경 시마다 갱신된 최대 길이 기록을 기준으로 계산합니다.
        ///
        /// 사용 예:
        /// @code
        /// float peak = s.peakUtilization();
        /// @endcode
        ///
        /// @return 0.0 ~ 100.0 사이의 최대 사용률 (High Water Mark)
        float peakUtilization() const;

        /// 버퍼의 전체 물리적 용량을 반환합니다.
        size_t capacity() const { return _capacity; }
        /// 현재 저장된 문자열의 바이트 길이를 반환합니다.
        size_t length() const { return _len; }
        /// 문자열이 비어있는지 확인합니다.
        bool isEmpty() const { return _len == 0; }
        /// C 스타일 문자열 포인터를 반환합니다.
        const char* c_str() const { return _buf; }
        /// const char* 타입으로의 암시적 형변환을 지원합니다.
        operator const char*() const { return _buf; }

        /// 문자열을 즉시 비웁니다.
        ///
        /// Why: 기존 데이터를 무효화하고 새로운 조립을 준비하기 위함입니다.
        /// How: 첫 바이트에 '\0'을 써서 논리적으로 초기화합니다.
        void clear();

        /// 특정 인덱스의 문자에 접근합니다.
        char& operator[](size_t index) { return _buf[index]; }
        /// 특정 인덱스의 문자에 접근합니다 (읽기 전용).
        const char& operator[](size_t index) const { return _buf[index]; }

        /// C 스타일 문자열을 대입합니다.
        ///
        /// Why: 기존 내용을 버리고 새로운 문자열로 교체하기 위함입니다.
        /// How: strlcpy를 사용하여 버퍼 크기를 초과하는 데이터는 안전하게 자릅니다.
        ///
        /// @param src 복사할 원본 문자열 포인터
        ///
        /// @return 자기 자신의 참조
        StringBase& operator=(const char* src);
        /// 다른 StringBase 객체의 내용을 복사 대입합니다.
        StringBase& operator=(const StringBase& other);
        /// Token 객체의 내용을 대입합니다.
        StringBase& operator=(const cms::string::Token& token);
        /// 문자열을 뒤에 결합합니다.
        StringBase& operator+=(const char* src);
        /// 단일 문자를 뒤에 결합합니다.
        StringBase& operator+=(char c);
        /// 다른 객체의 문자열을 뒤에 결합합니다.
        StringBase& operator+=(const StringBase& other);
        /// Token 객체의 문자열을 뒤에 결합합니다.
        StringBase& operator+=(const cms::string::Token& token);

        /// 기존 문자열 뒤에 지정된 길이만큼 데이터를 고속으로 덧붙입니다.
        ///
        /// Why: 버퍼 경계를 넘지 않으면서 데이터를 효율적으로 추가하기 위함입니다.
        /// How: 남은 공간을 계산하여 오버런을 방지하며 복사를 수행합니다.
        ///
        /// @param s 추가할 문자열 포인터
        /// @param len 추가할 바이트 길이
        ///
        /// @note len이 0이거나 s가 null이면 아무 작업도 하지 않습니다.
        void append(const char* s, size_t len);
        /// Token 객체의 데이터를 덧붙입니다.
        void append(const cms::string::Token& token);

        /// 문자열 양 끝의 공백 및 제어 문자를 제거합니다.
        ///
        /// Why: 데이터 정제 및 파싱 전처리를 위함입니다.
        /// How: 데이터를 앞으로 당겨 제자리에서 수정(In-place)합니다.
        void trim();

        /// 문자열이 특정 접두사로 시작하는지 확인합니다.
        ///
        /// @param prefix 찾고자 하는 접두사
        /// @param ignoreCase 대소문자 무시 여부
        ///
        /// @return true: 접두사 일치, false: 불일치
        bool startsWith(const char* prefix, bool ignoreCase = false) const;

        /// 문자열 리터럴 전용 접두사 확인 (최적화)
        /// Why: 컴파일 타임에 길이를 알 수 있어 strlen 호출을 생략하고 조기 종료가 가능합니다.
        template<size_t M>
        bool startsWith(const char (&prefix)[M], bool ignoreCase = false) const {
            if (M - 1 > _len) return false;
            return cms::string::startsWith(_buf, prefix, M - 1, ignoreCase);
        }

        /// 특정 문자열이 시작되는 논리적 글자 위치를 찾습니다.
        ///
        /// Why: UTF-8 인코딩 환경에서 정확한 글자 위치를 파악하기 위함입니다.
        /// How: 바이트 오프셋이 아닌 글자 인덱스를 계산하여 반환합니다.
        ///
        /// @param target 찾고자 하는 부분 문자열
        /// @param startChar 검색을 시작할 글자 위치 (범위: 0 ~ count())
        /// @param ignoreCase 대소문자 무시 여부
        ///
        /// @return 0부터 시작하는 글자 단위 인덱스 (찾지 못하면 -1)
        int find(const char* target, size_t startChar = 0, bool ignoreCase = false) const;

        /// 특정 문자가 처음 나타나는 논리적 위치를 찾습니다.
        int indexOf(char c, size_t startChar = 0, bool ignoreCase = false) const;
        /// 특정 문자열이 처음 나타나는 논리적 위치를 찾습니다.
        int indexOf(const char* str, size_t startChar = 0, bool ignoreCase = false) const;
        /// 문자열 리터럴 전용 indexOf (최적화)
        template<size_t M>
        int indexOf(const char (&str)[M], size_t startChar = 0, bool ignoreCase = false) const {
            return cms::string::find(_buf, _len, str, M - 1, startChar, ignoreCase);
        }

        /// 특정 문자열이 마지막으로 나타나는 위치를 찾습니다.
        int lastIndexOf(const char* target, bool ignoreCase = false) const;
        /// 문자열 리터럴 전용 lastIndexOf (최적화)
        template<size_t M>
        int lastIndexOf(const char (&target)[M], bool ignoreCase = false) const {
            return cms::string::lastIndexOf(_buf, _len, target, M - 1, ignoreCase);
        }
        /// 특정 문자가 마지막으로 나타나는 위치를 찾습니다.
        int lastIndexOf(char c, bool ignoreCase = false) const;

        /// 특정 문자열이 포함되어 있는지 확인합니다.
        bool contains(const char* target, bool ignoreCase = false) const;
        /// 문자열 리터럴 전용 contains (최적화)
        template<size_t M>
        bool contains(const char (&target)[M], bool ignoreCase = false) const {
            return cms::string::contains(_buf, _len, target, M - 1, ignoreCase);
        }

        /// POSIX 정규표현식 패턴과 일치하는지 검사합니다.
        bool matches(const char* pattern) const;

        /// 문자열이 특정 접미사로 끝나는지 확인합니다.
        bool endsWith(const char* suffix, bool ignoreCase = false) const;

        /// 문자열 리터럴 전용 접미사 확인 (최적화)
        /// Why: 컴파일 타임에 길이를 알 수 있어 strlen 호출을 생략하고 조기 종료가 가능합니다.
        template<size_t M>
        bool endsWith(const char (&suffix)[M], bool ignoreCase = false) const {
            if (M - 1 > _len) return false;
            return cms::string::endsWith(_buf, _len, suffix, M - 1, ignoreCase);
        }

        /// 문자열 내의 특정 패턴을 찾아 다른 문자열로 모두 치환합니다.
        ///
        /// Why: 텍스트 가공 및 템플릿 치환 기능을 제공하기 위함입니다.
        /// How: 치환 후 길이가 변할 경우 데이터를 재배치하며 버퍼 크기를 초과하면 중단됩니다.
        void replace(const char* from, const char* to, bool ignoreCase = false);

        /// 정수 값을 문자열로 변환하여 기존 내용 뒤에 덧붙입니다.
        ///
        /// Why: printf 계열보다 가볍고 빠른 전용 직렬화 로직을 사용하기 위함입니다.
        ///
        /// @param val 추가할 숫자 값
        /// @param width 최소 출력 너비 (단위: chars)
        /// @param padChar 채움 문자 (예: '0', ' ')
        void appendInt(long val, int width = 0, char padChar = ' ');
        /// 실수 값을 문자열로 변환하여 기존 내용 뒤에 덧붙입니다.
        void appendFloat(float val, int decimalPlaces = 2);

        /// 기존 내용을 모두 지우고 정수 값을 문자열로 설정합니다.
        void fromInt(long val);
        /// 기존 내용을 모두 지우고 실수 값을 문자열로 설정합니다.
        void fromFloat(float val, int decimalPlaces = 2);

        /// 가변 인자 리스트를 받아 포맷팅된 문자열을 기존 내용 뒤에 추가합니다.
        ///
        /// Why: 초경량 엔진을 사용하여 표준 vsnprintf 대비 스택 소모를 최소화하기 위함입니다.
        ///
        /// @param format printf 스타일 포맷 문자열
        /// @param args 가변 인자 리스트
        ///
        /// @return 포맷팅 후 최종 문자열의 전체 바이트 길이
        int appendPrintf(const char* format, va_list args);

        /// 가변 인자를 사용하여 포맷팅된 문자열을 버퍼에 씁니다.
        ///
        /// 사용 예:
        /// @code
        /// s.printf("Val: %d", 10);
        /// @endcode
        ///
        /// @param format printf 스타일 포맷 문자열
        ///
        /// @return 최종적으로 작성된 문자열의 바이트 길이
        int printf(const char* format, ...);

        /// 스트림 스타일로 문자열을 결합합니다.
        StringBase& operator<<(const char* s);

        /// 문자열 리터럴 전용 스트림 연산자입니다. (최적화)
        /// Why: 컴파일 타임에 길이를 알 수 있는 리터럴은 strlen 호출을 생략합니다.
        template<size_t M>
        StringBase& operator<<(const char (&s)[M]) {
            append(s, M - 1);
            return *this;
        }

        /// 문자열 리터럴 전용 결합 연산자입니다. (최적화)
        template<size_t M>
        StringBase& operator+=(const char (&s)[M]) {
            append(s, M - 1);
            return *this;
        }

        /// 스트림 스타일로 문자를 결합합니다.
        StringBase& operator<<(char c);
        /// 스트림 스타일로 정수를 결합합니다.
        StringBase& operator<<(int v);
        /// 스트림 스타일로 long 정수를 결합합니다.
        StringBase& operator<<(long v);
        /// 스트림 스타일로 unsigned long 정수를 결합합니다.
        StringBase& operator<<(unsigned long v);
        /// 스트림 스타일로 실수를 결합합니다.
        StringBase& operator<<(float v);
        /// 스트림 스타일로 double 실수를 결합합니다.
        StringBase& operator<<(double v);
        /// 스트림 스타일로 다른 객체의 내용을 결합합니다.
        StringBase& operator<<(const StringBase& other);
        /// 스트림 스타일로 Token 내용을 결합합니다.
        StringBase& operator<<(const cms::string::Token& token);

        /// 특정 글자 위치에 새로운 문자열을 끼워 넣습니다.
        ///
        /// How: memmove를 사용하여 기존 데이터를 뒤로 밀어내며 제자리에서 수정합니다.
        ///
        /// @param charIdx 삽입할 논리적 글자 위치 (범위: 0 ~ count())
        /// @param src 삽입할 문자열 포인터
        void insert(size_t charIdx, const char* src);
        /// 특정 글자 위치에 문자를 끼워 넣습니다.
        void insert(size_t charIdx, char c);

        /// 특정 구간의 글자들을 삭제하고 뒤쪽 데이터를 앞으로 당깁니다.
        ///
        /// @param charIdx 삭제를 시작할 글자 위치
        /// @param charCount 삭제할 글자 수
        void remove(size_t charIdx, size_t charCount);

        /// 문자열을 정수로 변환합니다.
        /// @return 변환된 정수값 (실패 시 0)
        int toInt() const;

        /// 구분자를 기준으로 문자열을 여러 토큰으로 분리합니다.
        ///
        /// Why: 프로토콜 패킷이나 CSV 데이터를 파싱하기 위함입니다.
        /// How: 원본 버퍼의 구분자 위치에 '\0'을 삽입하는 파괴적(Destructive) 방식을 사용합니다.
        ///
        /// @param delimiter 분리 기준 문자 (예: ':')
        /// @param tokens 분리된 문자열 포인터들을 저장할 배열
        /// @param maxTokens 배열의 최대 크기
        ///
        /// @return 실제 분리된 토큰의 개수
        size_t split(char delimiter, char** tokens, size_t maxTokens);

        /// 원본을 보존하며 문자열을 분리합니다. (비파괴적)
        size_t split(char delimiter, cms::string::Token* tokens, size_t maxTokens) const;

        /// 모든 영문을 대문자로 변환합니다.
        void toUpperCase();
        /// 모든 영문을 소문자로 변환합니다.
        void toLowerCase();

        /// 문자열의 논리적 글자 수를 반환합니다.
        /// @return 논리적 글자 수 (UTF-8 인식)
        size_t count() const;

        /// 현재 문자열이 표준 UTF-8 인코딩 규칙을 준수하는지 검증합니다.
        /// @return true: 유효한 UTF-8, false: 깨진 바이트 포함
        bool isValid() const;

        /// 버퍼 끝부분에 잘린 멀티바이트 문자가 있는지 검사하여 정제합니다.
        /// Why: 통신 중 데이터가 잘려 인코딩이 깨지는 것을 방지하기 위함입니다.
        void sanitize();



        /// 지정된 글자 범위를 추출하여 대상 객체에 저장합니다.
        ///
        /// @param dest 결과를 담을 StringBase 객체 참조
        /// @param left 시작 글자 인덱스 (범위: 0 ~ count())
        /// @param right 종료 글자 인덱스 (0일 경우 끝까지)
        void substring(StringBase& dest, size_t left, size_t right = 0) const;

        /// 물리적 바이트 오프셋을 기준으로 부분 문자열을 추출합니다.
        ///
        /// @param dest 결과를 담을 StringBase 객체 참조
        /// @param startByte 시작 바이트 위치 (범위: 0 ~ length())
        /// @param endByte 종료 바이트 위치 (0일 경우 끝까지)
        void byteSubstring(StringBase& dest, size_t startByte, size_t endByte = 0) const;

        /// 문자열 내용의 일치 여부를 확인합니다.
        bool equals(const char* other, bool ignoreCase = false) const;
        /// 문자열 비교 연산자입니다.
        bool operator==(const char* other) const {
            return equals(other);
        }
        /// 문자열 리터럴 비교 최적화 (컴파일 타임 길이를 활용하여 strcmp 호출 방지)
        template<size_t M>
        bool operator==(const char (&other)[M]) const {
            return cms::string::equals(_buf, _len, other, M - 1, false);
        }
        /// 문자열 불일치 연산자입니다.
        bool operator!=(const char* other) const { return !(*this == other); }
        /// 객체 간 비교 연산자입니다.
        bool operator==(const StringBase& other) const {
            return cms::string::equals(_buf, _len, other._buf, other._len, false);
        }
        /// 객체 간 불일치 연산자입니다.
        bool operator!=(const StringBase& other) const {
            return (_len != other._len) || !equals(other.c_str());
        }

        /// 외부 문자열과의 비교를 위한 프렌드 연산자입니다.
        friend bool operator==(const char* lhs, const StringBase& rhs);
        /// 외부 문자열 리터럴과의 비교 최적화
        template<size_t M>
        friend bool operator==(const char (&lhs)[M], const StringBase& rhs) {
            return cms::string::equals(lhs, M - 1, rhs._buf, rhs._len, false);
        }
        /// 외부 문자열과의 불일치를 위한 프렌드 연산자입니다.
        friend bool operator!=(const char* lhs, const StringBase& rhs);
        /// 외부 문자열 리터럴과의 불일치 최적화
        template<size_t M>
        friend bool operator!=(const char (&lhs)[M], const StringBase& rhs) {
            return !(lhs == rhs);
        }

    protected:
        /// 실제 문자열 데이터가 저장되는 외부 주입 메모리 버퍼의 시작 주소.
        char* const _buf;
        /// 버퍼의 물리적 최대 크기 (널 종료 문자 포함).
        const size_t _capacity;
        /// 현재 버퍼에 저장된 문자열의 바이트 길이 (널 종료 문자 제외).
        size_t _len;
        /// 객체 생성 이후 도달했던 최대 바이트 길이 (프로파일링용).
        size_t _maxLenSeen;

        /// 내부 생성자입니다. 자식 클래스에서 버퍼 정보를 주입받습니다.
        StringBase(char* b, size_t c);
        /// 길이를 명시적으로 지정하는 내부 생성자입니다. (최적화)
        StringBase(char* b, size_t c, size_t l);
        /// 현재 버퍼의 실제 문자열 길이를 측정하여 _len과 최대 사용량을 동기화합니다.
        void updateLength();
        /// 최대 사용량 지표를 갱신합니다.
        void updatePeak();
    };

    /// 외부 문자열과 객체의 비교 연산자입니다.
    bool operator==(const char* lhs, const StringBase& rhs);
    /// 외부 문자열과 객체의 불일치 연산자입니다.
    bool operator!=(const char* lhs, const StringBase& rhs);
}
#endif // CMS_STRING_BASE_H
