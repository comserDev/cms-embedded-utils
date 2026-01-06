// @author comser.dev
// =========================================================
// [cmsStringUtil.h] cms-embedded-utils 문자열 유틸리티의 핵심 선언부입니다.
// 제로 힙과 UTF-8 안전성을 유지하며, 인플레이스 처리를 기본으로 합니다.
// =========================================================

#pragma once

#include <cstring>  // strlen, strchr, strstr
#include <stddef.h> // size_t, NULL
#include <stdarg.h> // va_list

namespace cms {
    namespace string {
        // 표준 strlcpy가 없는 환경을 대비한 자체 구현 (BSD 스타일)
        size_t strlcpy(char *dst, const char *src, size_t dsize);

        // ---------------------------------------------------------
        // [strcasestr] 대소문자를 구분하지 않고 부분 문자열을 검색합니다.
        //
        // Usage: const char* p = cms::string::strcasestr(haystack, needle);
        //
        // @param haystack 검색 대상 문자열
        // @param needle 찾고자 하는 부분 문자열
        // @return 발견된 위치의 포인터 (찾지 못하면 nullptr)
        // ---------------------------------------------------------
        const char* strcasestr(const char* haystack, const char* needle);

        // ASCII 전용 대소문자 변환 인라인 함수
        // [최적화] 분기 없는(Branchless) 비트 연산을 사용하여 CPU 파이프라인 효율을 극대화합니다.
        inline char toLower(unsigned char c) noexcept {
            // c가 'A'~'Z' 사이에 있으면 mask는 1, 아니면 0이 됩니다.
            unsigned char mask = (unsigned char)((unsigned char)('A' - 1 - c) & (unsigned char)(c - ('Z' + 1))) >> 7;
            return (char)(c | (mask << 5));
        }

        inline char toUpper(unsigned char c) noexcept {
            // c가 'a'~'z' 사이에 있으면 mask는 1, 아니면 0이 됩니다.
            unsigned char mask = (unsigned char)((unsigned char)('a' - 1 - c) & (unsigned char)(c - ('z' + 1))) >> 7;
            return (char)(c & ~(mask << 5));
        }

        // ASCII 전용 숫자 확인 함수
        // [최적화] 분기 없는(Branchless) 비트 연산을 사용하여 성능을 향상시킵니다.
        inline bool isDigit(unsigned char c) noexcept {
            return (unsigned char)((unsigned char)('0' - 1 - c) & (unsigned char)(c - ('9' + 1))) >> 7;
        }

        // ASCII 전용 공백 문자 확인 함수 (Space, \t, \n, \v, \f, \r)
        // [최적화] 분기 없는(Branchless) 비트 연산을 사용하여 성능을 향상시킵니다.
        inline bool isSpace(unsigned char c) noexcept {
            unsigned char m1 = (unsigned char)((unsigned char)('\t' - 1 - c) & (unsigned char)(c - ('\r' + 1))) >> 7;
            unsigned char m2 = (unsigned char)((unsigned char)(' ' - 1 - c) & (unsigned char)(c - (' ' + 1))) >> 7;
            return (m1 | m2);
        }

        // ASCII 전용 16진수 문자 확인 함수 (0-9, a-f, A-F)
        // [최적화] 분기 없는(Branchless) 비트 연산을 사용하여 성능을 향상시킵니다.
        inline bool isHexDigit(unsigned char c) noexcept {
            unsigned char d = (unsigned char)((unsigned char)('0' - 1 - c) & (unsigned char)(c - ('9' + 1))) >> 7;
            unsigned char l = (unsigned char)((unsigned char)('a' - 1 - c) & (unsigned char)(c - ('f' + 1))) >> 7;
            unsigned char u = (unsigned char)((unsigned char)('A' - 1 - c) & (unsigned char)(c - ('F' + 1))) >> 7;
            return (d | l | u);
        }

        // ---------------------------------------------------------
        // [trim] 문자열 양 끝의 공백 및 제어 문자(\r, \n, \t)를 제거합니다.
        // 후방 공백은 널 문자로 자르고, 전방 공백은 memmove로 당깁니다.
        //
        // Usage: cms::string::trim(buf);
        //
        // @param str 수정할 대상 문자열 (Null-terminated)
        // @note 원본 문자열이 직접 수정되므로 데이터 보존이 필요하면 복사본을 사용하세요.
        // ---------------------------------------------------------
        size_t trim(char* str);

        // ---------------------------------------------------------
        // [startsWith] 문자열이 특정 접두사(prefix)로 시작하는지 확인합니다.
        // 접두사 길이만큼 메모리를 비교하며, 대소문자 무시 옵션을 지원합니다.
        //
        // Usage: if (cms::string::startsWith(s, "ACK_")) { ... }
        //
        // @param str 검사할 전체 문자열
        // @param prefix 찾고자 하는 접두사
        // @param ignoreCase true일 경우 대소문자를 구분하지 않고 비교합니다.
        // @return true: 접두사 일치, false: 불일치 또는 입력값 오류
        // ---------------------------------------------------------
        bool startsWith(const char* str, const char* prefix, bool ignoreCase = false);
        bool startsWith(const char* str, const char* prefix, size_t prefixLen, bool ignoreCase);

        // ---------------------------------------------------------
        // [equals] 두 문자열의 내용이 완전히 일치하는지 비교합니다.
        // 동일 포인터일 경우 즉시 참을 반환합니다.
        //
        // Usage: if (cms::string::equals(a, b)) { ... }
        //
        // @param s1 비교 대상 문자열 1
        // @param s2 비교 대상 문자열 2
        // @param ignoreCase true일 경우 대소문자를 무시하고 비교합니다.
        // @return true: 내용 일치, false: 불일치
        // ---------------------------------------------------------
        bool equals(const char* s1, const char* s2, bool ignoreCase = false);
        bool equals(const char* s1, size_t s1Len, const char* s2, size_t s2Len, bool ignoreCase);

        // ---------------------------------------------------------
        // [compare] 두 문자열을 사전식으로 비교합니다.
        //
        // @param s1 비교 대상 1
        // @param s1Len s1의 길이
        // @param s2 비교 대상 2
        // @param s2Len s2의 길이
        // @return 0: 일치, <0: s1이 작음, >0: s1이 큼
        // ---------------------------------------------------------
        int compare(const char* s1, size_t s1Len, const char* s2, size_t s2Len);
        int compareIgnoreCase(const char* s1, size_t s1Len, const char* s2, size_t s2Len);

        // ---------------------------------------------------------
        // [Token] 비파괴적 split 결과를 담는 구조체입니다.
        // 원본 문자열을 수정하지 않으므로 널 종료를 보장하지 않으며 길이 정보를 포함합니다.
        // ---------------------------------------------------------
        struct Token {
            const char* ptr;
            size_t len;

            /// 토큰의 내용을 정수(int)로 변환합니다.
            int toInt() const;
            /// 토큰의 내용을 실수(double)로 변환합니다.
            double toFloat() const;

            /// 다른 토큰과 내용을 비교합니다.
            bool equals(const Token& other, bool ignoreCase = false) const;
            /// 일반 문자열과 내용을 비교합니다.
            bool equals(const char* s, bool ignoreCase = false) const;

            /// 문자열 리터럴과 내용을 비교합니다. (최적화)
            template<size_t M>
            bool equals(const char (&s)[M], bool ignoreCase = false) const {
                return cms::string::equals(ptr, len, s, M - 1, ignoreCase);
            }

            // 편의를 위한 연산자 오버로딩
            bool operator==(const Token& other) const { return equals(other); }
            bool operator==(const char* s) const { return equals(s); }
            bool operator!=(const Token& other) const { return !equals(other); }
            bool operator!=(const char* s) const { return !equals(s); }
        };

        // ---------------------------------------------------------
        // [indexOf] 문자열 내에서 특정 문자가 처음 나타나는 위치를 찾습니다.
        //
        // Usage: const char* p = cms::string::indexOf(s, ':');
        //
        // @param str 검색 대상 문자열
        // @param c 찾을 문자 (ASCII 범위 권장)
        // @param ignoreCase true일 경우 대소문자를 구분하지 않습니다.
        // @return 문자가 발견된 위치의 포인터 (찾지 못하면 nullptr)
        // ---------------------------------------------------------
        const char* indexOf(const char* str, char c, bool ignoreCase = false);

        // ---------------------------------------------------------
        // [toInt] 숫자로 구성된 문자열을 정수(int)로 변환합니다.
        //
        // Usage: int v = cms::string::toInt("123");
        //
        // @param str 변환할 문자열 (예: "123")
        // @return 변환된 정수값 (실패 시 0)
        // ---------------------------------------------------------
        int toInt(const char* str);
        int toInt(const char* str, size_t len);

        // ---------------------------------------------------------
        // [isDigit] 문자열이 유효한 10진수 정수 형식인지 검사합니다.
        // 부호(+, -)와 앞뒤 공백을 허용합니다.
        //
        // @param str 검사할 문자열
        // @param len 문자열 길이 (0일 경우 내부에서 측정)
        // @return true: 유효한 정수 형식, false: 아님
        // ---------------------------------------------------------
        bool isDigit(const char* str);
        bool isDigit(const char* str, size_t len);

        // ---------------------------------------------------------
        // [hexToInt] 16진수 문자열을 정수(int)로 변환합니다.
        // "0x" 또는 "0X" 접두사를 지원합니다.
        //
        // Usage: int v = cms::string::hexToInt("0xABC");
        //
        // @param str 변환할 16진수 문자열
        // @return 변환된 정수값 (실패 시 0)
        // ---------------------------------------------------------
        int hexToInt(const char* str);
        int hexToInt(const char* str, size_t len);

        // ---------------------------------------------------------
        // [isHex] 문자열이 유효한 16진수 형식인지 검사합니다.
        // "0x" 또는 "0X" 접두사를 허용합니다.
        //
        // @param str 검사할 문자열
        // @param len 문자열 길이 (0일 경우 내부에서 측정)
        // @return true: 유효한 16진수, false: 유효하지 않음
        // ---------------------------------------------------------
        bool isHex(const char* str);
        bool isHex(const char* str, size_t len);

        // ---------------------------------------------------------
        // [toFloat] 숫자로 구성된 문자열을 실수(double)로 변환합니다.
        //
        // Usage: double v = cms::string::toFloat("3.14");
        //
        // @param str 변환할 문자열
        // @return 변환된 실수값 (실패 시 0.0)
        // ---------------------------------------------------------
        double toFloat(const char* str);
        double toFloat(const char* str, size_t len);

        // ---------------------------------------------------------
        // [isNumeric] 문자열이 유효한 실수(float/double) 형식인지 검사합니다.
        // 부호(+, -), 소수점(.), 앞뒤 공백을 허용합니다.
        //
        // @param str 검사할 문자열
        // @param len 문자열 길이 (0일 경우 내부에서 측정)
        // @return true: 유효한 숫자 형식, false: 아님
        // ---------------------------------------------------------
        bool isNumeric(const char* str);
        bool isNumeric(const char* str, size_t len);

        // ---------------------------------------------------------
        // [utf8_strlen] UTF-8 인코딩을 인식하여 문자열의 글자 수를 측정합니다.
        //
        // Usage: size_t n = cms::string::utf8_strlen(str);
        //
        // @param str 측정할 UTF-8 문자열
        // @return 논리적 글자 수 (바이트 크기가 아님)
        // ---------------------------------------------------------
        size_t utf8_strlen(const char* str);
        // ---------------------------------------------------------
        // [utf8SafeEnd] UTF-8 ?? ??? ???? ??? ?? ??? ?????.
        //
        // Usage: size_t end = cms::string::utf8SafeEnd(buf, start, maxBytes);
        //
        // @param str UTF-8 ???
        // @param startByte ?? ??? ??
        // @param maxBytes ?? ??? ??
        // @return ??? ???? ?? ??? ??(???)
        // ---------------------------------------------------------
        size_t utf8SafeEnd(const char* str, size_t startByte, size_t maxBytes);


        // ---------------------------------------------------------
        // [find] 문자열 내에서 특정 대상이 시작되는 글자 위치를 찾습니다.
        //
        // Usage: int idx = cms::string::find(s, "TAG");
        //
        // @param str 검색 대상 문자열
        // @param target 찾고자 하는 부분 문자열
        // @param startChar 검색을 시작할 글자 인덱스 (기본값: 0)
        // @param ignoreCase true일 경우 대소문자 무시
        // @return 0부터 시작하는 글자 단위 인덱스 (찾지 못하면 -1)
        // ---------------------------------------------------------
        int find(const char* str, const char* target, size_t startChar = 0, bool ignoreCase = false);
        int find(const char* str, size_t strLen, const char* target, size_t targetLen, size_t startChar, bool ignoreCase);

        // ---------------------------------------------------------
        // [lastIndexOf] 문자열에서 마지막으로 나타나는 위치를 찾습니다.
        //
        // Usage: int idx = cms::string::lastIndexOf(s, "END");
        //
        // @param str 검색 대상 문자열
        // @param target 찾고자 하는 부분 문자열
        // @param ignoreCase true일 경우 대소문자 무시
        // @return 마지막으로 발견된 글자 단위 인덱스 (찾지 못하면 -1)
        // ---------------------------------------------------------
        int lastIndexOf(const char* str, const char* target, bool ignoreCase = false);
        int lastIndexOf(const char* str, size_t strLen, const char* target, size_t targetLen, bool ignoreCase);

        // ---------------------------------------------------------
        // [insert] 특정 글자 위치에 문자열을 삽입합니다.
        //
        // Usage: cms::string::insert(buf, maxLen, 3, "ABC");
        //
        // @param buffer 대상 버퍼
        // @param maxLen 버퍼의 물리적 최대 크기 (널 종료 문자 포함)
        // @param charIdx 삽입할 논리적 글자 위치
        // @param src 삽입할 문자열
        // @return 삽입 후의 새로운 문자열 바이트 길이
        // ---------------------------------------------------------
        size_t insert(char* buffer, size_t maxLen, size_t curLen, size_t charIdx, const char* src);

        // ---------------------------------------------------------
        // [remove] 문자열의 특정 구간을 삭제합니다.
        //
        // Usage: cms::string::remove(buf, 0, 3);
        //
        // @param buffer 대상 버퍼
        // @param curLen 현재 문자열 길이
        // @param charIdx 삭제를 시작할 글자 위치
        // @param charCount 삭제할 글자 수
        // @return 삭제 후의 새로운 문자열 바이트 길이
        // ---------------------------------------------------------
        size_t remove(char* buffer, size_t curLen, size_t charIdx, size_t charCount);

        // ---------------------------------------------------------
        // [substring] 지정된 글자 범위를 추출합니다.
        //
        // Usage: cms::string::substring(src, out, outLen, 0, 5);
        //
        // @param src 원본 문자열
        // @param dest 결과를 저장할 버퍼
        // @param destLen 결과 버퍼의 최대 크기
        // @param left 시작 글자 인덱스
        // @param right 종료 글자 인덱스 (0일 경우 끝까지 추출)
        // @return 추출된 문자열의 바이트 길이
        // ---------------------------------------------------------
        size_t substring(const char* src, char* dest, size_t destLen, size_t left, size_t right = 0);

        // ---------------------------------------------------------
        // [byteSubstring] 바이트 오프셋 기준으로 문자열을 자릅니다.
        //
        // Usage: cms::string::byteSubstring(src, out, outLen, 0, 8);
        //
        // @param src 원본 문자열
        // @param dest 결과를 저장할 버퍼
        // @param destLen 결과 버퍼의 최대 크기
        // @param startByte 시작 바이트 오프셋
        // @param endByte 종료 바이트 오프셋 (0일 경우 끝까지)
        // @return 추출된 문자열의 바이트 길이
        // ---------------------------------------------------------
        size_t byteSubstring(const char* src, char* dest, size_t destLen, size_t startByte, size_t endByte = 0);

        // ---------------------------------------------------------

        // ---------------------------------------------------------
        // [split] 구분자로 문자열을 분리합니다. (파괴적)
        //
        // Usage: size_t n = cms::string::split(buf, ':', tokens, 4);
        //
        // @param str 분리할 원본 문자열 (함수 실행 후 수정됨)
        // @param delimiter 구분 문자 (예: ':')
        // @param tokens 분리된 문자열 포인터들을 저장할 배열
        // @param maxTokens 최대 분리 가능 개수
        // @return 실제 분리된 토큰의 개수
        // ---------------------------------------------------------
        size_t split(char* str, char delimiter, char** tokens, size_t maxTokens);

        // ---------------------------------------------------------
        // [split] 원본을 보존하는 비파괴적 분할 함수입니다. (최적화)
        //
        // Usage: size_t n = cms::string::split(buf, ':', tokens, 4);
        //
        // @param str 분리할 원본 문자열 (수정되지 않음)
        // @param delimiter 구분 문자
        // @param tokens 분리된 Token 구조체 배열
        // @param maxTokens 최대 분리 가능 개수
        // @return 실제 분리된 토큰의 개수
        // ---------------------------------------------------------
        size_t split(const char* str, char delimiter, Token* tokens, size_t maxTokens);

        // ---------------------------------------------------------
        // [contains] 문자열 내에 특정 부분 문자열이 포함되어 있는지 확인합니다.
        //
        // Usage: if (cms::string::contains(s, "ERR")) { ... }
        //
        // @param str 검사 대상 문자열
        // @param target 찾을 문자열
        // @param ignoreCase true일 경우 대소문자 무시
        // @return true: 포함됨, false: 미포함
        // ---------------------------------------------------------
        bool contains(const char* str, const char* target, bool ignoreCase = false);
        bool contains(const char* str, size_t strLen, const char* target, size_t targetLen, bool ignoreCase);

        // ---------------------------------------------------------
        // [toUpperCase] 영문 소문자를 대문자로 변환합니다.
        //
        // Usage: cms::string::toUpperCase(buf);
        //
        // @param str 변환할 대상 문자열 (In-place 수정)
        // ---------------------------------------------------------
        void toUpperCase(char* str);

        // ---------------------------------------------------------
        // [toLowerCase] 영문 대문자를 소문자로 변환합니다.
        //
        // Usage: cms::string::toLowerCase(buf);
        //
        // @param str 변환할 대상 문자열 (In-place 수정)
        // ---------------------------------------------------------
        void toLowerCase(char* str);

        // ---------------------------------------------------------
        // [endsWith] 문자열이 특정 접미사로 끝나는지 확인합니다.
        //
        // Usage: if (cms::string::endsWith(s, ".bin")) { ... }
        //
        // @param str 검사 대상 문자열
        // @param suffix 찾을 접미사
        // @param ignoreCase true일 경우 대소문자 무시
        // @return true: 일치, false: 불일치
        // ---------------------------------------------------------
        bool endsWith(const char* str, const char* suffix, bool ignoreCase = false);
        bool endsWith(const char* str, size_t strLen, const char* suffix, size_t suffixLen, bool ignoreCase);

        // ---------------------------------------------------------
        // [replace] 문자열 내의 특정 패턴을 모두 치환합니다.
        //
        // Usage: cms::string::replace(buf, maxLen, "A", "B");
        //
        // @param str 대상 문자열 (In-place 수정)
        // @param maxLen 버퍼의 물리적 최대 크기
        // @param curLen 현재 문자열 길이
        // @param from 찾을 패턴
        // @param to 바꿀 내용
        // @param ignoreCase true일 경우 대소문자 무시
        // @return 치환 완료 후의 새로운 문자열 바이트 길이
        // ---------------------------------------------------------
        size_t replace(char* str, size_t maxLen, size_t curLen, const char* from, const char* to, bool ignoreCase = false);

        // ---------------------------------------------------------
        // [matches] 정규식 패턴과의 일치 여부를 확인합니다.
        //
        // Usage: if (cms::string::matches(s, "^[0-9]+$")) { ... }
        //
        // @param str 검사 대상 문자열
        // @param pattern 정규표현식 패턴 (예: "^[0-9]+$")
        // @return true: 매칭 성공, false: 매칭 실패 또는 패턴 문법 오류
        // ---------------------------------------------------------
        bool matches(const char* str, const char* pattern);

        // ---------------------------------------------------------
        // [validateUtf8] 문자열이 UTF-8 규칙을 만족하는지 검사합니다.
        //
        // Usage: if (!cms::string::validateUtf8(s)) { ... }
        //
        // @param str 검사할 문자열
        // @return true: 유효한 UTF-8, false: 인코딩 오류(깨진 글자) 발견
        // ---------------------------------------------------------
        bool validateUtf8(const char* str);

        // ---------------------------------------------------------
        // [sanitizeUtf8] 깨진 UTF-8 바이트를 대체 문자로 치환합니다.
        //
        // Usage: cms::string::sanitizeUtf8(buf, maxLen);
        //
        // @param str 정제할 문자열 (In-place 수정)
        // @param maxLen 버퍼 최대 크기
        // @return 정제 후의 최종 문자열 바이트 길이
        // ---------------------------------------------------------
        size_t sanitizeUtf8(char* str, size_t maxLen);

        // ---------------------------------------------------------
        // [appendPrintf] 초경량 포맷팅 엔진입니다.
        //
        // Usage: cms::string::appendPrintf(buf, maxLen, "%d", args);
        //
        // @param buffer 결과가 저장될 버퍼
        // @param maxLen 버퍼의 최대 크기 (널 종료 문자 포함)
        // @param curLen 현재 문자열 길이 (참조로 전달되어 업데이트됨)
        // @param format 포맷 문자열
        // @param args 가변 인자 리스트 (va_list)
        // @return 포맷팅 완료 후 최종 문자열의 전체 바이트 길이
        // ---------------------------------------------------------
        int appendPrintf(char* buffer, size_t maxLen, size_t& curLen, const char* format, va_list args);

        // ---------------------------------------------------------
        // [append] 길이를 아는 데이터를 버퍼 끝에 추가합니다.
        //
        // Usage: cms::string::append(buf, maxLen, len, src, srcLen);
        //
        // @param buffer 대상 버퍼
        // @param maxLen 버퍼 최대 크기
        // @param curLen 현재 문자열 길이 (함수 실행 후 증가된 길이로 업데이트됨)
        // @param src 추가할 데이터 소스
        // @param srcLen 추가할 데이터의 바이트 길이
        // ---------------------------------------------------------
        void append(char* buffer, size_t maxLen, size_t& curLen, const char* src, size_t srcLen) noexcept;

        // ---------------------------------------------------------
        // [appendInt] 정수값을 문자열로 변환하여 버퍼 끝에 추가합니다.
        //
        // Usage: cms::string::appendInt(buf, maxLen, len, 42);
        //
        // @param buffer 대상 버퍼
        // @param maxLen 버퍼 최대 크기
        // @param curLen 현재 길이 (업데이트됨)
        // @param val 변환할 정수값 (long 타입)
        // @param width 최소 출력 너비 (0일 경우 가변 길이)
        // @param padChar 채움 문자 (예: '0', ' ')
        // ---------------------------------------------------------
        void appendInt(char* buffer, size_t maxLen, size_t& curLen, long val, int width = 0, char padChar = ' ');

        // ---------------------------------------------------------
        // [appendFloat] 실수값을 문자열로 변환하여 버퍼 끝에 추가합니다.
        //
        // Usage: cms::string::appendFloat(buf, maxLen, len, 3.14f, 2);
        //
        // @param buffer 대상 버퍼
        // @param maxLen 버퍼 최대 크기
        // @param curLen 현재 길이 (업데이트됨)
        // @param val 변환할 실수값 (float 타입)
        // @param decimalPlaces 소수점 이하 출력 자리수
        // ---------------------------------------------------------
        void appendFloat(char* buffer, size_t maxLen, size_t& curLen, double val, int decimalPlaces = 2);
    } // string
} // namespace cms

// =========================================================
// [User Guidelines]
// =========================================================
// 1) 대부분의 함수가 원본 문자열을 직접 수정합니다.
// 2) 입력 문자열은 반드시 '\0'으로 종료되어야 합니다.
// 3) 큰 버퍼는 전역 또는 static으로 두는 것이 안전합니다.
// 4) UTF-8 안전 함수(find/substring/insert)를 우선 사용하세요.
