// @author comser.dev
// ==================================================================================================
// [cmsStringUtil.cpp] cms-embedded-utils 문자열 유틸리티 구현 파일입니다.
// 문자열 처리(분리/치환/포맷팅/대소문자 변환)와 UTF-8 안전 처리(길이 계산/검증/정리)를 제공합니다.
//
// @note 모든 함수는 버퍼 길이(maxLen)를 넘지 않도록 동작하며, 문자열은 NUL 종료를 유지합니다.
// ==================================================================================================

#include <cstring>     // strlen, strstr, memcpy, memmove
#include <cstdlib>     // strtol
#include <sys/types.h> // regex_t 타입
#include <cstdint>     // uint64_t

#ifdef ARDUINO
// Arduino는 기본 환경에 정규식이 없으므로 POSIX regex를 명시적으로 포함합니다.
#include <regex.h>     // regcomp, regexec, regfree
#endif


#include "cmsStringUtil.h"   // cms::string 선언

namespace {
    // 소수점 처리를 위한 10의 거듭제곱 테이블 (최대 9자리)
    static const double powersOf10[] = {
        1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0, 100000000.0, 1000000000.0
    };

    // 반올림 보정을 위한 테이블 (0.5 / 10^n)
    static const double roundingOffsets[] = {
        0.5, 0.05, 0.005, 0.0005, 0.00005, 0.000005, 0.0000005, 0.00000005, 0.000000005, 0.0000000005
    };

    // --------------------------------------------------------------------------------------------------
    // [appendUIntInternal] 부호 없는 정수를 문자열로 변환하여 추가합니다.
    // --------------------------------------------------------------------------------------------------
    // Usage: appendUIntInternal(...);
    void appendUIntInternal(char* buffer, size_t maxLen, size_t& curLen, unsigned long uval, int width, char padChar) {
        // 1. 숫자 자릿수 계산 (분기 예측 최적화)
        int digitsCount;
        if (uval < 10) digitsCount = 1;
        else if (uval < 100) digitsCount = 2;
        else if (uval < 1000) digitsCount = 3;
        else if (uval < 10000) digitsCount = 4;
        else if (uval < 100000) digitsCount = 5;
        else if (uval < 1000000) digitsCount = 6;
        else if (uval < 10000000) digitsCount = 7;
        else if (uval < 100000000) digitsCount = 8;
        else if (uval < 1000000000) digitsCount = 9;
        else digitsCount = 10;

        // 2. 전체 출력 길이 결정 (Padding 포함)
        int totalLen = (digitsCount > width) ? digitsCount : width;

        // 3. 버퍼 공간 확인
        if (curLen + totalLen >= maxLen) return;

        // 4. 끝 지점부터 역순으로 채우기 (Reverse 과정 생략)
        size_t startIdx = curLen;
        size_t writeIdx = curLen + totalLen;
        curLen = writeIdx;
        buffer[writeIdx] = '\0';

        unsigned long v = uval;
        // 2글자 단위 룩업 테이블 (나눗셈 횟수 50% 절감)
        static const char digitsTable[] =
            "0001020304050607080910111213141516171819"
            "2021222324252627282930313233343536373839"
            "4041424344454647484950515253545556575859"
            "6061626364656667686970717273747576777879"
            "8081828384858687888990919293949596979899";

        // 2자리씩 처리
        while (v >= 100) {
            unsigned int i = (v % 100) << 1;
            v /= 100;
            buffer[--writeIdx] = digitsTable[i + 1];
            buffer[--writeIdx] = digitsTable[i];
        }
        // 남은 1~2자리 처리
        if (v >= 10) {
            unsigned int i = v << 1;
            buffer[--writeIdx] = digitsTable[i + 1];
            buffer[--writeIdx] = digitsTable[i];
        } else {
            buffer[--writeIdx] = (char)(v + '0');
        }
        while (writeIdx > startIdx) {
            buffer[--writeIdx] = padChar;
        }
        }

        // [최적화] KMP 알고리즘을 위한 부분 일치 테이블(LPS) 생성 함수
        void computeLPS(const char* pat, size_t m, int16_t* lps) {
            size_t len = 0;
            lps[0] = 0;
            size_t i = 1;
            while (i < m) {
                if (cms::string::toLower((unsigned char)pat[i]) == cms::string::toLower((unsigned char)pat[len])) {
                    len++;
                    lps[i] = (int16_t)len;
                    i++;
                } else {
                    if (len != 0) {
                        len = lps[len - 1];
                    } else {
                        lps[i] = 0;
                        i++;
                    }
                }
            }
    }

    // --------------------------------------------------------------------------------------------------
    // [appendHexInternal] 부호 없는 정수를 16진수 문자열로 변환하여 추가합니다.
    // --------------------------------------------------------------------------------------------------
    void appendHexInternal(char* buffer, size_t maxLen, size_t& curLen, unsigned long uval, int width, char padChar, bool uppercase) {
        // 1. 16진수 자릿수 계산 (비트 연산 활용)
        int digitsCount = 0;
        unsigned long temp = uval;
        if (temp == 0) digitsCount = 1;
        else {
            while (temp > 0) {
                temp >>= 4; // 16으로 나누기
                digitsCount++;
            }
        }

        // 2. 전체 출력 길이 결정
        int totalLen = (digitsCount > width) ? digitsCount : width;
        if (curLen + totalLen >= maxLen) return;

        // 3. 버퍼 공간 확보 및 NUL 종료
        size_t startIdx = curLen;
        size_t writeIdx = curLen + totalLen;
        curLen = writeIdx;
        buffer[writeIdx] = '\0';

        // 4. 역순으로 16진수 문자 채우기
        const char* hexChars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        unsigned long v = uval;
        for (int i = 0; i < digitsCount; ++i) {
            buffer[--writeIdx] = hexChars[v & 0xF];
            v >>= 4;
        }
        while (writeIdx > startIdx) {
            buffer[--writeIdx] = padChar;
        }
    }

    // --------------------------------------------------------------------------------------------------
    // [findUtf8CharStart] UTF-8 문자열에서 논리적 글자 인덱스에 해당하는 물리적 주소를 찾습니다.
    // 바이트 비트 패턴을 분석하여 멀티바이트 문자의 시작점을 정확히 판별합니다.
    //
    // Usage: findUtf8CharStart(...);
    //
    // @param str 검색할 UTF-8 문자열
    // @param charIdx 찾고자 하는 글자의 논리적 위치 (0부터 시작)
    // @return 해당 글자가 시작되는 메모리 주소 (범위 초과 시 문자열 끝 주소)
    // --------------------------------------------------------------------------------------------------
    const char* findUtf8CharStart(const char* str, size_t charIdx) {
        // 1. 입력 문자열이 유효하지 않으면 즉시 종료합니다.
        if (!str) return nullptr;

        const char* p = str;
        size_t count = 0;

        // 2. 문자열의 끝을 만나거나, 원하는 문자 인덱스(charIdx)에 도달할 때까지 바이트를 훑습니다.
        while (*p && count < charIdx) {
            // UTF-8 규칙: 바이트의 상위 2비트가 '10'이면 이전 문자에 딸린 '후속 바이트'입니다.
            // 따라서 '10'이 아닌 경우에만 새로운 문자가 시작된 것으로 보고 카운트를 올립니다.
            if ((*p & 0xC0) != 0x80) count++;
            p++;
        }
        // 3. 만약 루프가 끝났는데 p가 후속 바이트 중간을 가리키고 있다면, 다음 문자의 시작점까지 전진합니다.
        while (*p && (*p & 0xC0) == 0x80) p++;
        return p;
    }
}

namespace cms {
    namespace string {
        // 표준 strlcpy가 없는 환경을 대비한 자체 구현 (BSD 스타일)
        size_t strlcpy(char *dst, const char *src, size_t dsize) {
            const char *osrc = src;
            size_t nleft = dsize;

            // 버퍼 사이즈가 0이 아니면 복사 시작
            if (nleft != 0) {
                while (--nleft != 0) {
                    if ((*dst++ = *src++) == '\0')
                        break;
                }
            }

            // 버퍼가 꽉 찼다면 강제로 널 문자 삽입
            if (nleft == 0) {
                if (dsize != 0)
                    *dst = '\0'; // dst는 이미 끝을 가리키고 있음
                while (*src++); // src의 남은 길이를 계산하기 위해 끝까지 이동
            }

            return (src - osrc - 1); // 복사하려고 시도했던 원본 길이 반환
        }
        // [최적화] strcasestr을 대체하는 경량 대소문자 무시 검색 함수
        const char* strcasestr(const char* haystack, const char* needle) {
            if (!*needle) return haystack;
            size_t m = strlen(needle);

            // [최적화] 패턴이 짧은 경우 KMP 알고리즘 적용 (LPS 테이블 스택 할당)
            if (m <= 64) {
                int16_t lps[64];
                computeLPS(needle, m, lps);

                size_t i = 0; // haystack index
                size_t j = 0; // needle index
                while (haystack[i]) {
                    if (cms::string::toLower((unsigned char)haystack[i]) == cms::string::toLower((unsigned char)needle[j])) {
                        i++; j++;
                    }
                    if (j == m) return &haystack[i - m];
                    else if (haystack[i] && cms::string::toLower((unsigned char)haystack[i]) != cms::string::toLower((unsigned char)needle[j])) {
                        if (j != 0) j = lps[j - 1];
                        else i++;
                    }
                }
            } else {
                // 패턴이 너무 긴 경우(64바이트 초과) 스택 보호를 위해 Naive 방식으로 폴백
                char first = cms::string::toLower((unsigned char)*needle);
                for (; *haystack; haystack++) {
                    if (cms::string::toLower((unsigned char)*haystack) == first) {
                        const char* h = haystack + 1;
                        const char* n = needle + 1;
                        while (*h && *n && cms::string::toLower((unsigned char)*h) == cms::string::toLower((unsigned char)*n)) {
                            h++; n++;
                        }
                        if (!*n) return haystack;
                    }
                }
            }
            return nullptr;
        }

        bool Token::equals(const Token& other, bool ignoreCase) const {
            return cms::string::equals(ptr, len, other.ptr, other.len, ignoreCase);
        }

        bool Token::equals(const char* s, bool ignoreCase) const {
            if (!s) return false;
            return cms::string::equals(ptr, len, s, strlen(s), ignoreCase);
        }

        int Token::toInt() const {
            return cms::string::toInt(ptr, len);
        }

        double Token::toFloat() const {
            return cms::string::toFloat(ptr, len);
        }

        // --------------------------------------------------------------------------------------------------
        // [trim] 문자열 양 끝의 공백 및 제어 문자(\r, \n, \t)를 제거합니다.
        // 후방 공백은 널 문자로 자르고, 전방 공백은 memmove를 통해 데이터를 앞으로 당겨 제자리에서 수정합니다.
        //
        // --------------------------------------------------------------------------------------------------
        // Usage: trim(...);
        //
        // @param str 수정할 대상 문자열 (Null-terminated)
        // @note 원본 문자열이 직접 수정되므로 데이터 보존이 필요하면 복사본을 사용하세요.
        // --------------------------------------------------------------------------------------------------
        size_t trim(char* str) {
            if (!str || *str == '\0') return 0;

            size_t len = strlen(str);
            char* start = str;
            while (*start && cms::string::isSpace((unsigned char)*start)) {
                start++;
            }

            // 모든 문자가 공백인 경우 처리
            if (*start == '\0') {
                str[0] = '\0';
                return 0;
            }

            char* end = str + len - 1;
            while (end > start && cms::string::isSpace((unsigned char)*end)) {
                *end = '\0';
                end--;
            }

            size_t newLen = (end - start) + 1;
            if (start != str) {
                // [최적화] 이미 계산된 end와 start 포인터를 이용하여 이동할 길이를 즉시 도출
                memmove(str, start, newLen + 1);
            }

            return newLen;
        }

        // --------------------------------------------------------------------------------------------------
        // [startsWith] 문자열이 특정 접두사(prefix)로 시작하는지 확인합니다.
        // 접두사 길이만큼 메모리를 비교하며, 대소문자 무시 옵션을 지원합니다.
        //
        // Usage: startsWith(...);
        //
        // @param str 검사할 전체 문자열
        // @param prefix 찾고자 하는 접두사
        // @param ignoreCase true일 경우 대소문자를 구분하지 않고 비교합니다.
        // @return true: 접두사 일치, false: 불일치 또는 입력값 오류
        // --------------------------------------------------------------------------------------------------
        bool startsWith(const char* str, const char* prefix, bool ignoreCase) {
            if (!str || !prefix) return false;
            return startsWith(str, prefix, strlen(prefix), ignoreCase);
        }

        // [최적화] 길이를 이미 알고 있는 경우를 위한 오버로드
        bool startsWith(const char* str, const char* prefix, size_t prefixLen, bool ignoreCase) {
            if (!str || !prefix) return false;
            if (!ignoreCase) return strncmp(str, prefix, prefixLen) == 0;

            // [최적화] strncasecmp 대체 로직
            for (size_t i = 0; i < prefixLen; i++) {
                if (!str[i]) return false;
                if (toLower((unsigned char)str[i]) != toLower((unsigned char)prefix[i])) return false;
            }
            return true;
        }

        // --------------------------------------------------------------------------------------------------
        // [equals] 두 문자열의 내용이 완전히 일치하는지 비교합니다.
        // 포인터 주소 비교를 우선 수행하여 동일 객체일 경우 즉시 참을 반환하여 성능을 최적화합니다.
        //
        // Usage: equals(...);
        //
        // @param s1 비교 대상 문자열 1
        // @param s2 비교 대상 문자열 2
        // @param ignoreCase true일 경우 대소문자를 무시하고 비교합니다.
        // @return true: 내용 일치, false: 불일치
        // --------------------------------------------------------------------------------------------------
        bool equals(const char* s1, const char* s2, bool ignoreCase) {
            if (s1 == s2) return true;
            if (!s1 || !s2) return false;
            return equals(s1, strlen(s1), s2, strlen(s2), ignoreCase);
        }

        // [최적화] 길이를 이미 알고 있는 경우를 위한 오버로드
        bool equals(const char* s1, size_t s1Len, const char* s2, size_t s2Len, bool ignoreCase) {
            // 1. 길이 비교: 길이가 다르면 절대 같을 수 없음 (O(1) 최적화)
            if (s1Len != s2Len) return false;
            if (s1 == s2) return true;
            if (!s1 || !s2) return false;

            // 2. 내용 비교: 길이가 같으므로 memcmp/strncasecmp로 고속 비교
            if (!ignoreCase) return memcmp(s1, s2, s1Len) == 0;

            // [최적화] strncasecmp 대체 로직
            for (size_t i = 0; i < s1Len; i++) {
                if (toLower((unsigned char)s1[i]) != toLower((unsigned char)s2[i])) return false;
            }
            return true;
        }

        // --------------------------------------------------------------------------------------------------
        // [compare] 두 문자열을 사전식으로 비교합니다. (UTF-8 안전)
        // --------------------------------------------------------------------------------------------------
        int compare(const char* s1, size_t s1Len, const char* s2, size_t s2Len) {
            if (s1 == s2) return 0;
            if (!s1) return -1;
            if (!s2) return 1;

            size_t minLen = (s1Len < s2Len) ? s1Len : s2Len;
            int res = memcmp(s1, s2, minLen);
            if (res != 0) return res;

            if (s1Len < s2Len) return -1;
            if (s1Len > s2Len) return 1;
            return 0;
        }

        // --------------------------------------------------------------------------------------------------
        // [compareIgnoreCase] 대소문자를 무시하고 두 문자열을 사전식으로 비교합니다.
        // --------------------------------------------------------------------------------------------------
        int compareIgnoreCase(const char* s1, size_t s1Len, const char* s2, size_t s2Len) {
            if (s1 == s2) return 0;
            if (!s1) return -1;
            if (!s2) return 1;

            size_t minLen = (s1Len < s2Len) ? s1Len : s2Len;
            const unsigned char* p1 = reinterpret_cast<const unsigned char*>(s1);
            const unsigned char* p2 = reinterpret_cast<const unsigned char*>(s2);

            for (size_t i = 0; i < minLen; i++) {
                unsigned char c1 = p1[i];
                unsigned char c2 = p2[i];

                // [최적화] Fast-Path: 두 바이트가 완전히 같으면 변환 없이 즉시 통과
                if (c1 != c2) {
                    unsigned char lc1 = toLower(c1);
                    unsigned char lc2 = toLower(c2);
                    if (lc1 != lc2) return (int)lc1 - (int)lc2;
                }
            }

            if (s1Len < s2Len) return -1;
            if (s1Len > s2Len) return 1;
            return 0;
        }

        // --------------------------------------------------------------------------------------------------
        // [indexOf] 문자열 내에서 특정 문자가 처음 나타나는 위치의 주소를 찾습니다.
        // 표준 strchr을 기반으로 하며, 대소문자 무시 시 루프를 통해 소문자 변환 비교를 수행합니다.
        //
        // Usage: indexOf(...);
        //
        // @param str 검색 대상 문자열
        // @param c 찾을 문자 (ASCII 범위 권장)
        // @param ignoreCase true일 경우 대소문자를 구분하지 않습니다.
        // @return 문자가 발견된 위치의 포인터 (찾지 못하면 nullptr)
        // --------------------------------------------------------------------------------------------------
        const char* indexOf(const char* str, char c, bool ignoreCase) {
            if (!str) return nullptr;

            // 1. 대소문자 무시 검색:
            if (ignoreCase) {
                // 비교의 일관성을 위해 찾고자 하는 대상 문자를 미리 소문자로 변환해 둡니다.
                char target = toLower((unsigned char)c);
                // 문자열의 끝('\0')을 만날 때까지 한 바이트씩 순회하며 검사합니다.
                while (*str) {
                    // 현재 글자를 소문자로 변환하여 대상 문자와 일치하는지 확인합니다.
                    if (toLower((unsigned char)*str) == target) {
                        return str; // 일치하는 위치의 메모리 주소를 즉시 반환합니다.
                    }
                    str++;
                }
                return nullptr; // 끝까지 찾지 못한 경우 NULL을 반환합니다.
            }

            // 2. 대소문자 구분 검색: 표준 라이브러리의 최적화된 strchr 함수를 사용하여 고속 검색합니다.
            return strchr(str, c);
        }

        // --------------------------------------------------------------------------------------------------
        // [toInt] 숫자로 구성된 문자열을 정수(int)로 변환합니다.
        // strtol을 사용하여 10진수 기반으로 안전하게 변환하며, 오버플로우 시 long 범위를 따릅니다.
        //
        // Usage: toInt(...);
        //
        // @param str 변환할 문자열 (예: "123")
        // @return 변환된 정수값 (실패 시 0)
        // --------------------------------------------------------------------------------------------------
        int toInt(const char* str) {
            if (!str || *str == '\0') return 0;
            return toInt(str, strlen(str));
        }

        // [최적화] NUL 종료되지 않은 문자열(Token 등)을 위한 정수 변환 로직
        int toInt(const char* str, size_t len) {
            if (!str || len == 0) return 0;

            size_t i = 0;
            // 1. 앞부분 공백 건너뛰기
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            if (i == len) return 0;

            // 2. 부호 처리
            int sign = 1;
            if (str[i] == '-') { sign = -1; i++; }
            else if (str[i] == '+') { i++; }

            // 3. 숫자 파싱
            long long val = 0;
            while (i < len && cms::string::isDigit((unsigned char)str[i])) {
                unsigned char c = (unsigned char)str[i];
                val = val * 10 + (c - '0');
                i++;
            }

            return static_cast<int>(val * sign);
        }

        // --------------------------------------------------------------------------------------------------
        // [isDigit] 문자열이 유효한 10진수 정수 형식인지 검사합니다.
        // --------------------------------------------------------------------------------------------------
        bool isDigit(const char* str) {
            if (!str || *str == '\0') return false;
            return isDigit(str, strlen(str));
        }

        bool isDigit(const char* str, size_t len) {
            if (!str || len == 0) return false;

            size_t i = 0;
            // 1. 앞부분 공백 건너뛰기
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            if (i == len) return false;

            // 2. 부호 처리
            if (str[i] == '+' || str[i] == '-') i++;

            size_t digitCount = 0;
            // 3. 숫자 본체 검사
            while (i < len && cms::string::isDigit((unsigned char)str[i])) {
                i++;
                digitCount++;
            }

            // 4. 뒷부분 공백 허용 및 최종 유효성 판단
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            return (i == len && digitCount > 0);
        }

        // --------------------------------------------------------------------------------------------------
        // [hexToInt] 16진수 문자열을 정수로 변환합니다.
        // --------------------------------------------------------------------------------------------------
        int hexToInt(const char* str) {
            if (!str || *str == '\0') return 0;
            return hexToInt(str, strlen(str));
        }

        int hexToInt(const char* str, size_t len) {
            if (!str || len == 0) return 0;

            size_t i = 0;
            // 1. 앞부분 공백 건너뛰기
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            if (i == len) return 0;

            // 2. 0x 또는 0X 접두사 처리
            if (i + 1 < len && str[i] == '0' && toLower((unsigned char)str[i+1]) == 'x') {
                i += 2;
            }

            unsigned int val = 0;
            while (i < len && isHexDigit((unsigned char)str[i])) {
                unsigned char c = (unsigned char)str[i++];
                val <<= 4;
                // [최적화] 이미 isHexDigit을 통과했으므로 isDigit 결과에 따라
                // 숫자('0'-'9') 또는 문자('a'-'f'/'A'-'F') 값을 안전하게 더합니다.
                if (cms::string::isDigit(c)) val += (c - '0');
                else val += (cms::string::toLower(c) - 'a' + 10);
            }
            return static_cast<int>(val);
        }

        // --------------------------------------------------------------------------------------------------
        // [isHex] 문자열이 유효한 16진수 형식인지 검사합니다.
        // --------------------------------------------------------------------------------------------------
        bool isHex(const char* str) {
            if (!str || *str == '\0') return false;
            return isHex(str, strlen(str));
        }

        bool isHex(const char* str, size_t len) {
            if (!str || len == 0) return false;

            size_t i = 0;
            // 1. 앞부분 공백 건너뛰기
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            if (i == len) return false;

            // 2. 0x 또는 0X 접두사 처리
            if (i + 1 < len && str[i] == '0' && toLower((unsigned char)str[i+1]) == 'x') {
                i += 2;
            }

            size_t digitCount = 0;
            // 3. 16진수 숫자 본체 검사
            while (i < len && isHexDigit((unsigned char)str[i])) {
                i++;
                digitCount++;
            }

            // 4. 뒷부분 공백 허용 및 최종 유효성 판단
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            return (i == len && digitCount > 0);
        }

        // --------------------------------------------------------------------------------------------------
        // [toFloat] 문자열을 실수로 변환합니다.
        // --------------------------------------------------------------------------------------------------
        double toFloat(const char* str) {
            if (!str || *str == '\0') return 0.0;
            return toFloat(str, strlen(str));
        }

        // [최적화] NUL 종료되지 않은 문자열을 위한 실수 변환 로직
        double toFloat(const char* str, size_t len) {
            if (!str || len == 0) return 0.0;

            size_t i = 0;
            // 1. 공백 스킵
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            if (i == len) return 0.0;

            // 2. 부호 처리
            double sign = 1.0;
            if (str[i] == '-') { sign = -1.0; i++; }
            else if (str[i] == '+') { i++; }

            // 3. 정수부 파싱
            double val = 0.0;
            while (i < len && cms::string::isDigit((unsigned char)str[i])) {
                unsigned char c = (unsigned char)str[i];
                val = val * 10.0 + (c - '0');
                i++;
            }

            // 4. 소수부 파싱
            if (i < len && str[i] == '.') {
                i++;
                double weight = 0.1;
                while (i < len && cms::string::isDigit((unsigned char)str[i])) {
                    unsigned char c = (unsigned char)str[i];
                    val += (c - '0') * weight;
                    weight /= 10.0;
                    i++;
                }
            }

            return val * sign;
        }

        // --------------------------------------------------------------------------------------------------
        // [isNumeric] 문자열이 유효한 실수 형식인지 검사합니다.
        // --------------------------------------------------------------------------------------------------
        bool isNumeric(const char* str) {
            if (!str || *str == '\0') return false;
            return isNumeric(str, strlen(str));
        }

        bool isNumeric(const char* str, size_t len) {
            if (!str || len == 0) return false;

            size_t i = 0;
            // 1. 앞부분 공백 건너뛰기
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            if (i == len) return false;

            // 2. 부호 처리
            if (str[i] == '+' || str[i] == '-') i++;

            size_t digitCount = 0;
            bool hasDot = false;

            // 3. 숫자 및 소수점 검사
            while (i < len) {
                unsigned char c = (unsigned char)str[i];
                if (cms::string::isDigit(c)) {
                    digitCount++;
                } else if (str[i] == '.') {
                    if (hasDot) return false; // 소수점은 하나만 허용
                    hasDot = true;
                } else {
                    break;
                }
                i++;
            }

            // 4. 뒷부분 공백 허용 및 최종 유효성 판단
            while (i < len && cms::string::isSpace((unsigned char)str[i])) i++;
            return (i == len && digitCount > 0);
        }

        // --------------------------------------------------------------------------------------------------
        // [utf8_strlen] UTF-8 인코딩을 인식하여 문자열의 실제 글자 수를 측정합니다.
        // 멀티바이트 문자의 후속 바이트를 제외하고 시작 바이트만 카운트하여 논리적 길이를 도출합니다.
        //
        // Usage: utf8_strlen(...);
        //
        // @param str 측정할 UTF-8 문자열
        // @return 논리적 글자 수 (바이트 크기가 아님)
        // --------------------------------------------------------------------------------------------------
        size_t utf8_strlen(const char* str) {
            if (!str) return 0;

            const unsigned char* p = reinterpret_cast<const unsigned char*>(str);
            size_t count = 0;

            // 빠른 경로: 선행 ASCII 바이트가 연속되는 경우 한 번에 처리
            while (*p && *p < 0x80) { count++; p++; }
            if (!*p) return count;

            // 비 ASCII가 섞여 있는 경우 기존 방식으로 남은 부분 처리
            while (*p) {
                if ((*p & 0xC0) != 0x80) count++;
                p++;
            }
            return count;
        }

        // --------------------------------------------------------------------------------------------------
        // [utf8SafeEnd] UTF-8 문자열을 자를 때 글자 중간이 잘리지 않는 안전한 바이트 위치를 계산합니다.
        // 지정된 범위 끝에서 역방향으로 스캔하여 유효한 글자의 시작점을 찾습니다.
        //
        // Usage: size_t end = cms::string::utf8SafeEnd(str, 0, 10);
        //
        // @param str 원본 UTF-8 문자열
        // @param startByte 검색을 시작할 바이트 오프셋
        // @param maxBytes 허용되는 최대 바이트 길이
        // @return 잘리지 않는 안전한 종료 바이트 위치
        // --------------------------------------------------------------------------------------------------
        size_t utf8SafeEnd(const char* str, size_t startByte, size_t maxBytes) {
            if (!str || maxBytes == 0) return startByte;

            size_t len = strlen(str);
            if (startByte >= len) return len;

            size_t end = startByte + maxBytes;
            if (end > len) end = len;

            // 후속 바이트(10xxxxxx)인 동안 뒤로 이동하여 글자 경계 탐색
            while (end > startByte && (static_cast<unsigned char>(str[end]) & 0xC0) == 0x80) {
                --end;
            }
            return end;
        }

        // --------------------------------------------------------------------------------------------------
        // [find] 문자열 내에서 특정 대상(target)이 시작되는 논리적 글자 위치를 찾습니다.
        // 물리적 주소를 먼저 찾은 후, 시작점부터 해당 주소까지의 UTF-8 글자 수를 역산합니다.
        //
        // Usage: find(...);
        //
        // @param str 검색 대상 문자열
        // @param target 찾고자 하는 부분 문자열
        // @param startChar 검색을 시작할 글자 인덱스 (기본값: 0)
        // @param ignoreCase true일 경우 대소문자 무시
        // @return 0부터 시작하는 글자 단위 인덱스 (찾지 못하면 -1)
        // --------------------------------------------------------------------------------------------------
        int find(const char* str, const char* target, size_t startChar, bool ignoreCase) {
            if (!str || !target) return -1;
            return find(str, strlen(str), target, strlen(target), startChar, ignoreCase);
        }

        int find(const char* str, size_t strLen, const char* target, size_t targetLen, size_t startChar, bool ignoreCase) {
            if (!str || !target || targetLen == 0 || targetLen > strLen) return -1;

            // 1. 물리적 시작 주소 확보: n번째 '글자'가 시작되는 실제 메모리 주소를 계산합니다.
            const char* startPtr = findUtf8CharStart(str, startChar);
            if (!startPtr || *startPtr == '\0') return -1;

            // 2. 고속 메모리 스캔: strstr 또는 strcasestr을 사용하여 주소를 찾습니다.
            const char* foundPtr = ignoreCase ? cms::string::strcasestr(startPtr, target) : strstr(startPtr, target);
            if (!foundPtr) return -1;

            // 3. 논리적 인덱스 변환: startPtr부터 foundPtr까지의 글자 수를 계산하여 상대적 인덱스로 환산
            size_t charOffset = 0;
            for (const char* p = startPtr; p < foundPtr; ++p) {
                if ((*p & 0xC0) != 0x80) charOffset++;
            }
            return static_cast<int>(startChar + charOffset);
        }

        // --------------------------------------------------------------------------------------------------
        // [lastIndexOf] 문자열의 끝에서부터 역순으로 특정 단어를 검색하여 글자 위치를 반환합니다.
        // 전체 문자열을 순방향으로 반복 스캔하여 가장 마지막에 발견된 위치를 확정합니다.
        //
        // Usage: lastIndexOf(...);
        //
        // @param str 검색 대상 문자열
        // @param target 찾고자 하는 부분 문자열
        // @param ignoreCase true일 경우 대소문자 무시
        // @return 마지막으로 발견된 글자 단위 인덱스 (찾지 못하면 -1)
        // --------------------------------------------------------------------------------------------------
        int lastIndexOf(const char* str, const char* target, bool ignoreCase) {
            if (!str || !target) return -1;
            return lastIndexOf(str, strlen(str), target, strlen(target), ignoreCase);
        }

        int lastIndexOf(const char* str, size_t strLen, const char* target, size_t targetLen, bool ignoreCase) {
            if (!str || !target || targetLen == 0 || targetLen > strLen) return -1;

            const char* lastFound = nullptr;
            const char* current = str;

            while (true) {
                const char* found = ignoreCase ? cms::string::strcasestr(current, target) : strstr(current, target);
                if (!found) break;
                lastFound = found;
                current = found + 1; // 다음 검색은 발견된 위치 바로 다음부터 시작
            }

            if (!lastFound) return -1;

            // 처음부터 마지막 발견 지점까지 한 번만 스캔하여 인덱스 확정
            size_t charIdx = 0;
            for (const char* p = str; p < lastFound; ++p) {
                if ((*p & 0xC0) != 0x80) charIdx++;
            }
            return static_cast<int>(charIdx);
        }

        // --------------------------------------------------------------------------------------------------
        // [insert] 기존 문자열의 특정 글자 위치에 새로운 문자열을 끼워 넣습니다.
        // memmove를 사용하여 삽입 지점 이후의 데이터를 뒤로 밀어내며, 버퍼 크기를 초과하면 잘릴 수 있습니다.
        //
        // Usage: insert(...);
        //
        // @param buffer 대상 버퍼
        // @param maxLen 버퍼의 물리적 최대 크기 (널 종료 문자 포함)
        // @param charIdx 삽입할 논리적 글자 위치
        // @param src 삽입할 문자열
        // --------------------------------------------------------------------------------------------------
        size_t insert(char* buffer, size_t maxLen, size_t curLen, size_t charIdx, const char* src) {
            if (!buffer || !src || *src == '\0') return curLen;

            // 1. 삽입 지점 확보: 삽입할 글자 인덱스를 물리적 메모리 주소로 변환합니다.
            const char* targetPtr = findUtf8CharStart(buffer, charIdx);
            size_t byteOffset = targetPtr - buffer;
            size_t srcLen = strlen(src);

            // 2. 오버플로우 방어: 삽입 후 전체 길이가 버퍼 크기를 넘지 않도록 삽입할 길이를 조정합니다.
            if (curLen + srcLen >= maxLen) {
                srcLen = (maxLen > curLen + 1) ? (maxLen - curLen - 1) : 0;
            }
            if (srcLen == 0) return curLen;

            // 3. 데이터 밀기: [최적화] 삽입 위치가 끝이 아닐 때만 memmove 수행
            if (byteOffset < curLen) {
                memmove(buffer + byteOffset + srcLen, buffer + byteOffset, curLen - byteOffset + 1);
            } else {
                // 끝에 추가하는 경우 밀어낼 필요 없이 널 종료 문자 위치만 확정
                buffer[curLen + srcLen] = '\0';
            }

            // 4. 데이터 복사: 확보된 빈 공간에 새로운 문자열을 복사해 넣습니다.
            memcpy(buffer + byteOffset, src, srcLen);
            return curLen + srcLen;
        }

        // --------------------------------------------------------------------------------------------------
        // [remove] 문자열의 특정 구간을 삭제하고 뒤쪽 데이터를 앞으로 당깁니다.
        // 삭제할 글자 수만큼의 영역을 memmove로 덮어씌워 제자리에서 수정합니다.
        //
        // Usage: remove(...);
        //
        // @param buffer 대상 버퍼
        // @param charIdx 삭제를 시작할 글자 위치
        // @param charCount 삭제할 글자 수
        // --------------------------------------------------------------------------------------------------
        size_t remove(char* buffer, size_t curLen, size_t charIdx, size_t charCount) {
            if (!buffer) return 0;

            // 1. 삭제 범위 계산: 삭제를 시작할 위치와 끝낼 위치의 물리적 주소를 각각 찾습니다.
            const char* startPtr = findUtf8CharStart(buffer, charIdx);
            if (!startPtr || *startPtr == '\0') return curLen;

            const char* endPtr = findUtf8CharStart(buffer, charIdx + charCount);
            size_t startOffset = startPtr - buffer;
            size_t endOffset = endPtr - buffer;

            // 2. 데이터 당기기: 삭제 구간 뒤에 있는 데이터를 앞으로 당겨서 삭제 구간을 덮어씁니다.
            size_t tailLen = curLen - endOffset;
            memmove(buffer + startOffset, buffer + endOffset, tailLen + 1);
            return curLen - (endOffset - startOffset);
        }

        // --------------------------------------------------------------------------------------------------
        // [substring] 원본 문자열에서 지정된 글자 범위를 추출하여 대상 버퍼에 복사합니다.
        // UTF-8 경계를 인식하여 시작/종료 주소를 계산하므로 한글 중간이 잘리는 현상을 방지합니다.
        //
        // Usage: substring(...);
        //
        // @param src 원본 문자열
        // @param dest 결과를 저장할 버퍼
        // @param destLen 결과 버퍼의 최대 크기
        // @param left 시작 글자 인덱스
        // @param right 종료 글자 인덱스 (0일 경우 끝까지 추출)
        // --------------------------------------------------------------------------------------------------
        size_t substring(const char* src, char* dest, size_t destLen, size_t left, size_t right) {
            if (!src || !dest || destLen == 0) return 0;
            dest[0] = '\0';

            // 1. 추출 범위 계산: 잘라낼 시작점과 끝점의 물리적 주소를 찾습니다.
            const char* startPtr = findUtf8CharStart(src, left);
            if (!startPtr || *startPtr == '\0') return 0;

            const char* endPtr;
            if (right == 0) {
                endPtr = src + strlen(src);
            } else {
                if (right <= left) return 0;
                // 시작 지점(startPtr)부터 상대적으로 종료 지점 탐색 (중복 스캔 방지)
                endPtr = findUtf8CharStart(startPtr, right - left);
            }

            if (endPtr <= startPtr) return 0;

            // 2. 안전 복사: 대상 버퍼(dest)의 크기를 넘지 않도록 길이를 조절하여 복사합니다.
            size_t byteLen = endPtr - startPtr;
            if (byteLen >= destLen) byteLen = destLen - 1;
            memcpy(dest, startPtr, byteLen);
            dest[byteLen] = '\0';
            return byteLen;
        }

        // --------------------------------------------------------------------------------------------------
        // [byteSubstring] 논리적 글자가 아닌 물리적 바이트 오프셋을 기준으로 문자열을 자릅니다.
        // 고정 크기 프로토콜 패킷이나 바이너리 데이터가 섞인 문자열을 처리할 때 유용합니다.
        //
        // Usage: byteSubstring(...);
        //
        // @param src 원본 문자열
        // @param dest 결과를 저장할 버퍼
        // @param destLen 결과 버퍼의 최대 크기
        // @param startByte 시작 바이트 오프셋
        // @param endByte 종료 바이트 오프셋 (0일 경우 끝까지)
        // --------------------------------------------------------------------------------------------------
        size_t byteSubstring(const char* src, char* dest, size_t destLen, size_t startByte, size_t endByte) {
            if (!src || !dest || destLen == 0) return 0;
            dest[0] = '\0';

            // 1. 오프셋 유효성 검사: 시작 바이트가 원본 길이를 넘는지 확인합니다.
            size_t srcLen = strlen(src);
            if (startByte >= srcLen) return 0;
            // 끝 바이트가 0이거나 원본보다 크면 원본 끝으로 설정합니다.
            if (endByte == 0 || endByte > srcLen) endByte = srcLen;
            if (endByte <= startByte) return 0;

            // 2. 데이터 복사: 지정된 바이트 범위만큼 대상 버퍼로 복사합니다.
            size_t copyLen = endByte - startByte;
            if (copyLen >= destLen) copyLen = destLen - 1;

            memcpy(dest, src + startByte, copyLen);
            dest[copyLen] = '\0';
            return copyLen;
        }

        // --------------------------------------------------------------------------------------------------
        // [split] 구분자(delimiter)를 기준으로 문자열을 여러 조각으로 분리합니다.
        // 원본 문자열의 구분자 위치에 '\0'을 삽입하여 논리적으로 분리하는 파괴적(Destructive) 방식입니다.
        //
        // Usage: split(...);
        //
        // @param str 분리할 원본 문자열 (함수 실행 후 수정됨)
        // @param delimiter 구분 문자 (예: ':')
        // @param tokens 분리된 문자열 포인터들을 저장할 배열
        // @param maxTokens 최대 분리 가능 개수
        // @return 실제 분리된 토큰의 개수
        // --------------------------------------------------------------------------------------------------
        size_t split(char* str, char delimiter, char** tokens, size_t maxTokens) {
            // 1. 방어적 프로그래밍: 입력 포인터나 토큰 배열이 유효하지 않으면 0을 반환합니다.
            if (!str || !tokens || maxTokens == 0) return 0;

            // 2. 초기화: 첫 번째 조각의 시작점은 항상 원본 문자열의 시작 주소(0번 인덱스)입니다.
            size_t count = 0;
            tokens[count++] = str;

            // 3. 스캔 루프: 문자열의 끝('\0')을 만날 때까지 한 바이트씩 전진하며 구분자를 찾습니다.
            char* p = str;
            while (*p) {
                if (*p == delimiter) {
                    // 4. 구분자 발견: 아직 토큰 배열에 여유 공간이 있다면 분리 작업을 수행합니다.
                    if (count < maxTokens) {
                        // [In-place 수정]: 구분자 자리에 '\0'을 써서 이전 조각을 독립된 문자열로 만듭니다.
                        *p = '\0';
                        // 바로 다음 바이트의 주소를 다음 조각의 시작점으로 저장합니다.
                        tokens[count++] = p + 1;
                    } else {
                        // 배열이 꽉 찼다면 더 이상 자르지 않고 중단합니다. 마지막 조각은 남은 전체를 포함합니다.
                        break;
                    }
                }
                p++;
            }
            // 5. 결과 반환: 최종적으로 분리된 조각의 개수를 반환합니다.
            return count;
        }

        // --------------------------------------------------------------------------------------------------
        // [split] 비파괴적 분할 구현부
        // --------------------------------------------------------------------------------------------------
        size_t split(const char* str, char delimiter, Token* tokens, size_t maxTokens) {
            if (!str || !tokens || maxTokens == 0) return 0;

            size_t count = 0;
            const char* start = str;
            const char* p = str;

            while (*p) {
                if (*p == delimiter) {
                    if (count < maxTokens - 1) {
                        tokens[count].ptr = start;
                        tokens[count].len = (size_t)(p - start);
                        count++;
                        start = p + 1;
                    } else {
                        break; // 마지막 토큰은 남은 전체를 포함
                    }
                }
                p++;
            }

            // 마지막 세그먼트 추가
            tokens[count].ptr = start;
            tokens[count].len = (size_t)(p - start);
            return ++count;
        }

        // --------------------------------------------------------------------------------------------------
        // [append] 길이를 이미 알고 있는 데이터를 버퍼 끝에 고속으로 추가합니다.
        // strlen 호출 오버헤드를 제거하기 위해 memcpy를 사용하며, curLen을 참조로 받아 즉시 업데이트합니다.
        //
        // Usage: append(...);
        //
        // @param buffer 대상 버퍼
        // @param maxLen 버퍼 최대 크기
        // @param curLen 현재 문자열 길이 (함수 실행 후 증가된 길이로 업데이트됨)
        // @param src 추가할 데이터 소스
        // @param srcLen 추가할 데이터의 바이트 길이
        // --------------------------------------------------------------------------------------------------
        void append(char* __restrict buffer, size_t maxLen, size_t& curLen, const char* __restrict src, size_t srcLen) noexcept {
            if (!buffer || !src || srcLen == 0 || curLen >= maxLen - 1) return;

            size_t available = maxLen - 1 - curLen;
            size_t toCopy = (srcLen < available) ? srcLen : available;

            if (toCopy > 0) {
                memcpy(buffer + curLen, src, toCopy);
                curLen += toCopy;
                buffer[curLen] = '\0';
            }
        }

        // --------------------------------------------------------------------------------------------------
        // [appendInt] 정수값을 문자열로 변환하여 버퍼 끝에 추가합니다.
        // 숫자를 역순으로 추출한 뒤 해당 구간을 뒤집어 정렬하며, 자릿수 맞춤(Padding) 기능을 지원합니다.
        //
        // Usage: appendInt(...);
        //
        // @param buffer 대상 버퍼
        // @param maxLen 버퍼 최대 크기
        // @param curLen 현재 길이 (업데이트됨)
        // @param val 변환할 정수값 (long 타입)
        // @param width 최소 출력 너비 (0일 경우 가변 길이)
        // @param padChar 채움 문자 (예: '0', ' ')
        // --------------------------------------------------------------------------------------------------
        void appendInt(char* buffer, size_t maxLen, size_t& curLen, long val, int width, char padChar) {
            // 1. 방어적 코드: 버퍼가 이미 가득 찼다면 중단합니다.
            if (curLen >= maxLen - 1) return;
            unsigned long uval;
            if (val < 0) {
                buffer[curLen++] = '-';
                uval = static_cast<unsigned long>(-(val + 1)) + 1;
                appendUIntInternal(buffer, maxLen, curLen, uval, width > 0 ? width - 1 : 0, padChar);
            } else {
                uval = static_cast<unsigned long>(val);
                appendUIntInternal(buffer, maxLen, curLen, uval, width, padChar);
            }
        }

        // --------------------------------------------------------------------------------------------------
        // [appendFloat] 실수값을 문자열로 변환하여 버퍼 끝에 추가합니다.
        // 반올림 보정을 수행한 후 정수부와 소수부를 분리하여 순차적으로 결합합니다.
        //
        // Usage: appendFloat(...);
        //
        // @param buffer 대상 버퍼
        // @param maxLen 버퍼 최대 크기
        // @param curLen 현재 길이 (업데이트됨)
        // @param val 변환할 실수값 (float 타입)
        // @param decimalPlaces 소수점 이하 출력 자리수
        // --------------------------------------------------------------------------------------------------
        void appendFloat(char* buffer, size_t maxLen, size_t& curLen, double val, int decimalPlaces) {
            if (curLen >= maxLen - 1) return;
            if (decimalPlaces < 0) decimalPlaces = 0;
            if (decimalPlaces > 9) decimalPlaces = 9;

            double dval = val;
            if (dval < 0) {
                buffer[curLen++] = '-';
                dval = -dval;
            }

            // 1. 반올림 보정 (나눗셈 대신 미리 계산된 테이블 사용)
            dval += roundingOffsets[decimalPlaces];

            // 2. 정수부 추출 및 추가
            unsigned long intPart = (unsigned long)dval;
            appendInt(buffer, maxLen, curLen, (long)intPart, 0, ' ');

            // 3. 소수부 추출 및 추가 (정수 연산으로 변환하여 정밀도 확보)
            if (decimalPlaces > 0 && curLen < maxLen - 1) {
                buffer[curLen++] = '.';

                double fracPart = dval - (double)intPart;
                // 부동 소수점 오차 보정을 위해 미세한 값(epsilon)을 더함 (double 정밀도에 맞춰 조정)
                unsigned long fracInt = (unsigned long)(fracPart * powersOf10[decimalPlaces] + 1e-9);

                // 자릿수 오버플로우 방지 (예: 0.999... 가 1.0이 되는 경우)
                if (fracInt >= (unsigned long)powersOf10[decimalPlaces]) {
                    fracInt = (unsigned long)powersOf10[decimalPlaces] - 1;
                }

                appendUIntInternal(buffer, maxLen, curLen, fracInt, decimalPlaces, '0');
            }
        }

        // --------------------------------------------------------------------------------------------------
        // [contains] 문자열 내에 특정 부분 문자열이 포함되어 있는지 확인합니다.
        // strstr 또는 strcasestr을 사용하여 부분 일치 여부를 검사합니다.
        //
        // Usage: contains(...);
        //
        // @param str 검사 대상 문자열
        // @param target 찾을 문자열
        // @param ignoreCase true일 경우 대소문자 무시
        // @return true: 포함됨, false: 미포함
        // --------------------------------------------------------------------------------------------------
        bool contains(const char* str, const char* target, bool ignoreCase) {
            if (!str || !target) return false;
            return contains(str, strlen(str), target, strlen(target), ignoreCase);
        }

        bool contains(const char* str, size_t strLen, const char* target, size_t targetLen, bool ignoreCase) {
            if (!str || !target || targetLen > strLen) return false;
            if (targetLen == 0) return true;

            return (ignoreCase ? cms::string::strcasestr(str, target) : strstr(str, target)) != nullptr;
        }

        // --------------------------------------------------------------------------------------------------
        // [toUpperCase] 문자열의 모든 영문 소문자를 대문자로 변환합니다.
        // ASCII 범위(0~127) 내의 문자만 처리하여 멀티바이트(한글 등) 인코딩 깨짐을 방지합니다.
        //
        // --------------------------------------------------------------------------------------------------
        // Usage: toUpperCase(...);
        //
        // @param str 변환할 대상 문자열 (In-place 수정)
        // --------------------------------------------------------------------------------------------------
        void toUpperCase(char* str) {
            if (!str) return;
            while (*str) {
                *str = toUpper((unsigned char)*str);
                str++;
            }
        }

        // --------------------------------------------------------------------------------------------------
        // [toLowerCase] 문자열의 모든 영문 대문자를 소문자로 변환합니다.
        // ASCII 범위(0~127) 내의 문자만 처리하여 멀티바이트(한글 등) 인코딩 깨짐을 방지합니다.
        //
        // --------------------------------------------------------------------------------------------------
        // Usage: toLowerCase(...);
        //
        // @param str 변환할 대상 문자열 (In-place 수정)
        // --------------------------------------------------------------------------------------------------
        void toLowerCase(char* str) {
            if (!str) return;
            while (*str) {
                *str = toLower((unsigned char)*str);
                str++;
            }
        }

        // --------------------------------------------------------------------------------------------------
        // [endsWith] 문자열이 특정 접미사(suffix)로 끝나는지 확인합니다.
        // 문자열의 끝 지점에서 접미사 길이만큼 역산한 위치부터 비교를 수행합니다.
        //
        // Usage: endsWith(...);
        //
        // @param str 검사 대상 문자열
        // @param suffix 찾을 접미사
        // @param ignoreCase true일 경우 대소문자 무시
        // @return true: 일치, false: 불일치
        // --------------------------------------------------------------------------------------------------
        bool endsWith(const char* str, const char* suffix, bool ignoreCase) {
            if (!str || !suffix) return false;
            return endsWith(str, strlen(str), suffix, strlen(suffix), ignoreCase);
        }

        // [최적화] 길이를 이미 알고 있는 경우를 위한 오버로드
        bool endsWith(const char* str, size_t strLen, const char* suffix, size_t suffixLen, bool ignoreCase) {
            if (!str || !suffix || suffixLen > strLen) return false;

            const char* target = str + (strLen - suffixLen);
            if (!ignoreCase) return strcmp(target, suffix) == 0;

            // [최적화] strcasecmp 대체 로직
            while (*target && *suffix) {
                if (toLower((unsigned char)*target) != toLower((unsigned char)*suffix)) return false;
                target++; suffix++;
            }
            return *suffix == '\0';
        }

        // --------------------------------------------------------------------------------------------------
        // [replace] 문자열 내의 특정 패턴(from)을 찾아 다른 문자열(to)로 모두 치환합니다.
        // 치환 후 길이가 변할 경우 memmove를 통해 데이터를 재배치하며, 버퍼 크기를 초과하면 중단됩니다.
        //
        // Usage: replace(...);
        //
        // @param str 대상 문자열 (In-place 수정)
        // @param maxLen 버퍼의 물리적 최대 크기
        // @param from 찾을 패턴
        // @param to 바꿀 내용
        // @param ignoreCase true일 경우 대소문자 무시
        // --------------------------------------------------------------------------------------------------
        size_t replace(char* str, size_t maxLen, size_t curLen, const char* from, const char* to, bool ignoreCase) {
            if (!str || !from || !to || *from == '\0') return curLen;

            size_t fromLen = strlen(from);
            size_t toLen = strlen(to);
            size_t currentLen = curLen;
            char* p = str;
            bool truncated = false;

            while (true) {
                // [최적화] 단일 문자 검색 시 strchr 사용 (대소문자 구분 시)
                if (ignoreCase) {
                    p = (char*) cms::string::strcasestr(p, from);
                } else {
                    p = (fromLen == 1) ? strchr(p, *from) : strstr(p, from);
                }

                if (!p) break;

                if (toLen > fromLen) {
                    size_t diff = toLen - fromLen;
                    if (currentLen + diff >= maxLen) {
                        truncated = true;
                        break;
                    }
                    // [최적화] 데이터가 늘어나는 경우만 memmove로 공간 확보
                    memmove(p + toLen, p + fromLen, currentLen - ((p - str) + fromLen) + 1);
                    currentLen += diff;
                } else if (toLen < fromLen) {
                    // [최적화] 데이터가 줄어드는 경우 memmove로 간격 좁힘
                    size_t diff = fromLen - toLen;
                    memmove(p + toLen, p + fromLen, currentLen - ((p - str) + fromLen) + 1);
                    currentLen -= diff;
                }
                // toLen == fromLen 인 경우는 memmove 없이 바로 memcpy 수행

                memcpy(p, to, toLen);
                p += toLen;
            }

            // [최적화] 실제로 버퍼 오버플로우로 인해 잘린 경우에만 UTF-8 정제 수행
            return truncated ? sanitizeUtf8(str, maxLen) : currentLen;
        }

        // --------------------------------------------------------------------------------------------------
        // [matches] POSIX 정규표현식을 사용하여 문자열이 특정 패턴에 부합하는지 검사합니다.
        // 내부적으로 regcomp/regexec를 사용하며, 실행 후 할당된 정규식 자원을 즉시 해제합니다.
        //
        // Usage: matches(...);
        //
        // @param str 검사 대상 문자열
        // @param pattern 정규표현식 패턴 (예: "^[0-9]+$")
        // @return true: 매칭 성공, false: 매칭 실패 또는 패턴 문법 오류
        // --------------------------------------------------------------------------------------------------
        bool matches(const char* str, const char* pattern) {
#ifdef ARDUINO
// Arduino는 기본 환경에 정규식이 없으므로 POSIX regex를 명시적으로 포함합니다.
            // 1. 유효성 검사: 대상 문자열이나 패턴이 비어있으면 매칭 실패로 간주합니다.
            if (!str || !pattern || *str == '\0') return false;

            // 2. 정규식 객체 선언: POSIX 표준 정규식 정보를 담을 구조체입니다.
            regex_t regex;

            // 3. [컴파일 단계]: 텍스트 패턴을 엔진이 실행 가능한 바이너리 형태로 변환합니다.
            // REG_EXTENDED: 확장 문법 사용, REG_NOSUB: 매칭 여부만 확인하여 메모리를 절약합니다.
            int ret = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);

            // 4. 컴파일 에러 체크: 패턴 문법에 오류가 있다면 즉시 false를 반환합니다.
            if (ret != 0) return false;

            // 5. [실행 단계]: 컴파일된 정규식을 실제 문자열에 적용하여 일치 여부를 확인합니다.
            // regexec는 패턴이 일치하면 0을 반환합니다.
            ret = regexec(&regex, str, 0, NULL, 0);

            // 6. [정리 단계]: 컴파일 과정에서 할당된 내부 메모리를 해제하여 메모리 누수를 방지합니다.
            regfree(&regex);

            // 7. 결과 반환: 0이면 일치(true), 그 외에는 불일치(false)입니다.
            return (ret == 0);
#else
            return false; // Native 환경에서는 정규식 테스트 제외
#endif
        }

        // --------------------------------------------------------------------------------------------------
        // [validateUtf8] 문자열이 올바른 UTF-8 인코딩 규칙을 따르는지 전수 조사합니다.
        // 각 바이트의 비트 패턴을 분석하여 유효한 1~4바이트 시퀀스인지 검증합니다.
        //
        // Usage: validateUtf8(...);
        //
        // @param str 검사할 문자열
        // @return true: 유효한 UTF-8, false: 인코딩 오류(깨진 글자) 발견
        // --------------------------------------------------------------------------------------------------
        bool validateUtf8(const char* str) {
            if (!str) return false;

            const unsigned char* bytes = reinterpret_cast<const unsigned char*>(str);

            while (*bytes) {
                // 1. [1바이트 영역 (ASCII)]: 00~7F 범위는 단일 바이트 글자입니다.
                if (bytes[0] <= 0x7F) {
                    bytes++;
                }
                // 2. [2바이트 영역]: C2~DF로 시작하며, 뒤에 1개의 후속 바이트가 와야 합니다.
                else if (bytes[0] >= 0xC2 && bytes[0] <= 0xDF) {
                    if ((bytes[1] & 0xC0) != 0x80) return false;
                    bytes += 2;
                }
                // 3. [3바이트 영역]: 한글 등이 포함되는 영역입니다.
                else if (bytes[0] == 0xE0) {
                    // Overlong Encoding 방지: 더 짧게 표현 가능한 문자를 길게 쓴 경우를 차단합니다.
                    if (bytes[1] < 0xA0 || bytes[1] > 0xBF || (bytes[2] & 0xC0) != 0x80) return false;
                    bytes += 3;
                }
                else if (bytes[0] >= 0xE1 && bytes[0] <= 0xEC) {
                    if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) return false;
                    bytes += 3;
                }
                else if (bytes[0] == 0xED) {
                    // Surrogate Pair 방지: 유니코드 대행 쌍 영역(U+D800~U+DFFF)은 UTF-8에서 유효하지 않습니다.
                    if (bytes[1] < 0x80 || bytes[1] > 0x9F || (bytes[2] & 0xC0) != 0x80) return false;
                    bytes += 3;
                }
                else if (bytes[0] >= 0xEE && bytes[0] <= 0xEF) {
                    if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) return false;
                    bytes += 3;
                }
                // 4. [4바이트 영역]: 이모지 등이 포함되는 4바이트 글자 영역입니다.
                else if (bytes[0] == 0xF0) {
                    if (bytes[1] < 0x90 || bytes[1] > 0xBF || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) return false;
                    bytes += 4;
                }
                else if (bytes[0] >= 0xF1 && bytes[0] <= 0xF3) {
                    if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) return false;
                    bytes += 4;
                }
                else if (bytes[0] == 0xF4) {
                    // 유니코드 최대값(U+10FFFF) 초과 방지: 물리적 한계를 넘는 시퀀스를 차단합니다.
                    if (bytes[1] < 0x80 || bytes[1] > 0x8F || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) return false;
                    bytes += 4;
                }
                else {
                    // 어떤 규칙에도 맞지 않는 잘못된 바이트입니다.
                    return false;
                }
            }
            return true;
        }

        // --------------------------------------------------------------------------------------------------
        // [sanitizeUtf8] 문자열 내의 깨진 UTF-8 바이트를 찾아 유니코드 대체 문자()로 치환합니다.
        // 비정상적인 바이트 시퀀스로 인해 터미널 출력이 멈추거나 시스템이 오작동하는 것을 방지합니다.
        //
        // Usage: sanitizeUtf8(...);
        //
        // @param str 정제할 문자열 (In-place 수정)
        // @param maxLen 버퍼 최대 크기
        // --------------------------------------------------------------------------------------------------
        size_t sanitizeUtf8(char* str, size_t maxLen) {
            if (!str || maxLen == 0) return 0;

            unsigned char* src = reinterpret_cast<unsigned char*>(str);
            unsigned char* dst = src;
            const char* replacement = "\xEF\xBF\xBD"; // U+FFFD 대체문자(UTF-8 바이트 시퀀스)
            const size_t replLen = 3;

            // 한 번의 선형 통과로 문자열을 재작성하여 O(n) 동작 보장
            while (*src) {
                // 유효한 UTF-8 시퀀스와 길이를 검사합니다
                if (src[0] <= 0x7F) {
                    // ASCII 한 바이트
                    if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) >= maxLen - 1) break;
                    *dst++ = *src++;
                } else if (src[0] >= 0xC2 && src[0] <= 0xDF) {
                    // 2-byte sequence
                    if (src[1] && (src[1] & 0xC0) == 0x80) {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + 2 >= maxLen) break;
                        *dst++ = *src++;
                        *dst++ = *src++;
                    } else {
                        // 유효하지 않음: 대체문자(혹은 공간 부족 시 '?') 로 대체
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                            memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                        } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                            *dst++ = '?'; src += 1;
                        } else break;
                    }
                } else if (src[0] == 0xE0) {
                    if (src[1] && src[2] && src[1] >= 0xA0 && src[1] <= 0xBF && (src[2] & 0xC0) == 0x80) {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + 3 >= maxLen) break;
                        *dst++ = *src++; *dst++ = *src++; *dst++ = *src++;
                    } else {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                            memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                        } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                            *dst++ = '?'; src += 1;
                        } else break;
                    }
                } else if (src[0] >= 0xE1 && src[0] <= 0xEC) {
                    if (src[1] && src[2] && (src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80) {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + 3 >= maxLen) break;
                        *dst++ = *src++; *dst++ = *src++; *dst++ = *src++;
                    } else {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                            memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                        } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                            *dst++ = '?'; src += 1;
                        } else break;
                    }
                } else if (src[0] == 0xED) {
                    if (src[1] && src[2] && src[1] >= 0x80 && src[1] <= 0x9F && (src[2] & 0xC0) == 0x80) {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + 3 >= maxLen) break;
                        *dst++ = *src++; *dst++ = *src++; *dst++ = *src++;
                    } else {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                            memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                        } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                            *dst++ = '?'; src += 1;
                        } else break;
                    }
                } else if (src[0] >= 0xEE && src[0] <= 0xEF) {
                    if (src[1] && src[2] && (src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80) {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + 3 >= maxLen) break;
                        *dst++ = *src++; *dst++ = *src++; *dst++ = *src++;
                    } else {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                            memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                        } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                            *dst++ = '?'; src += 1;
                        } else break;
                    }
                } else if (src[0] == 0xF0) {
                    if (src[1] && src[2] && src[3] && src[1] >= 0x90 && src[1] <= 0xBF && (src[2] & 0xC0) == 0x80 && (src[3] & 0xC0) == 0x80) {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + 4 >= maxLen) break;
                        *dst++ = *src++; *dst++ = *src++; *dst++ = *src++; *dst++ = *src++;
                    } else {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                            memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                        } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                            *dst++ = '?'; src += 1;
                        } else break;
                    }
                } else if (src[0] >= 0xF1 && src[0] <= 0xF3) {
                    if (src[1] && src[2] && src[3] && (src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80 && (src[3] & 0xC0) == 0x80) {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + 4 >= maxLen) break;
                        *dst++ = *src++; *dst++ = *src++; *dst++ = *src++; *dst++ = *src++;
                    } else {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                            memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                        } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                            *dst++ = '?'; src += 1;
                        } else break;
                    }
                } else if (src[0] == 0xF4) {
                    if (src[1] && src[2] && src[3] && src[1] >= 0x80 && src[1] <= 0x8F && (src[2] & 0xC0) == 0x80 && (src[3] & 0xC0) == 0x80) {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + 4 >= maxLen) break;
                        *dst++ = *src++; *dst++ = *src++; *dst++ = *src++; *dst++ = *src++;
                    } else {
                        if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                            memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                        } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                            *dst++ = '?'; src += 1;
                        } else break;
                    }
                } else {
                    // 어떤 규칙에도 맞지 않는 바이트
                    if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) + replLen < maxLen) {
                        memcpy(dst, replacement, replLen); dst += replLen; src += 1;
                    } else if ((size_t)(dst - reinterpret_cast<unsigned char*>(str)) < maxLen - 1) {
                        *dst++ = '?'; src += 1;
                    } else break;
                }
            }

            // 최종 널 종료
            size_t finalLen = (size_t)(dst - reinterpret_cast<unsigned char*>(str));
            if (finalLen < maxLen) *dst = '\0';
            else { finalLen = maxLen - 1; *(reinterpret_cast<unsigned char*>(str) + finalLen) = '\0'; }

            return finalLen;
        }

        // --------------------------------------------------------------------------------------------------
        // [appendPrintf] 표준 vsnprintf를 대체하는 초경량 포맷팅 엔진입니다.
        // 스택 사용량을 최소화하며 %s, %d, %ld, %f, %c, %%, %02d 등 필수 포맷만 직접 파싱하여 처리합니다.
        //
        // Usage: appendPrintf(...);
        //
        // @param buffer 결과가 저장될 버퍼
        // @param maxLen 버퍼의 최대 크기 (널 종료 문자 포함)
        // @param format 포맷 문자열
        // @param args 가변 인자 리스트 (va_list)
        // @return 포맷팅 완료 후 최종 문자열의 전체 바이트 길이
        // --------------------------------------------------------------------------------------------------
        int appendPrintf(char* buffer, size_t maxLen, size_t& curLen, const char* format, va_list args) {
            if (!buffer || !format) return 0;

            const char* p = format;
            while (*p) {
                // [최적화] 다음 포맷 지정자(%) 위치를 찾아 리터럴 텍스트를 일괄 복사
                const char* nextPercent = strchr(p, '%');
                if (nextPercent != p) {
                    size_t literalLen = (nextPercent) ? (size_t)(nextPercent - p) : strlen(p);
                    append(buffer, maxLen, curLen, p, literalLen);
                    p += literalLen;
                    if (!*p) break;
                }

                // p는 이제 '%'를 가리킴
                if (*(p + 1)) {
                    p++; // '%' 문자 건너뛰기
                    char padChar = ' ';
                    int width = 0;
                    int precision = -1; // 정밀도 변수 추가

                    // 1. 플래그 확인 (예: '0'이 오면 0으로 채움)
                    if (*p == '0') {
                        padChar = '0';
                        p++;
                    }

                    // 2. 너비(Width) 파싱 (예: %02d에서 '2')
                    while (cms::string::isDigit((unsigned char)*p)) {
                        unsigned char c = (unsigned char)*p;
                        if (width < 100) width = width * 10 + (c - '0'); // 비정상적인 너비 제한
                        p++;
                    }

                    // 3. 정밀도(Precision) 파싱 (예: %.3f)
                    if (*p == '.') {
                        p++;
                        precision = 0;
                        while (cms::string::isDigit((unsigned char)*p)) {
                            unsigned char c = (unsigned char)*p;
                            precision = precision * 10 + (c - '0');
                            p++;
                        }
                    }

                    // 4. 타입별 처리
                    switch (*p) {
                        case 's': { // 문자열
                            const char* s = va_arg(args, const char*);
                            const char* src = s ? s : "(null)";
                            append(buffer, maxLen, curLen, src, strlen(src));
                            break;
                        }
                        case 'd': // 정수
                            appendInt(buffer, maxLen, curLen, (long)va_arg(args, int), width, padChar);
                            break;
                        case 'u': // 부호 없는 정수
                            appendUIntInternal(buffer, maxLen, curLen, va_arg(args, unsigned int), width, padChar);
                            break;
                        case 'x': // 16진수 (소문자)
                            appendHexInternal(buffer, maxLen, curLen, va_arg(args, unsigned int), width, padChar, false);
                            break;
                        case 'X': // 16진수 (대문자)
                            appendHexInternal(buffer, maxLen, curLen, va_arg(args, unsigned int), width, padChar, true);
                            break;
                        case 'l': // long (ld, lu, lx, lX 대응)
                            if (*(p + 1) == 'd') {
                                appendInt(buffer, maxLen, curLen, va_arg(args, long), width, padChar);
                                p++;
                            } else if (*(p + 1) == 'u') {
                                appendUIntInternal(buffer, maxLen, curLen, va_arg(args, unsigned long), width, padChar);
                                p++;
                            } else if (*(p + 1) == 'x') {
                                appendHexInternal(buffer, maxLen, curLen, va_arg(args, unsigned long), width, padChar, false);
                                p++;
                            } else if (*(p + 1) == 'X') {
                                appendHexInternal(buffer, maxLen, curLen, va_arg(args, unsigned long), width, padChar, true);
                                p++;
                            }
                            break;
                        case 'f': // 실수
                            appendFloat(buffer, maxLen, curLen, va_arg(args, double), (precision >= 0) ? precision : 2);
                            break;
                        case 'c': // 단일 문자
                            {
                                char c = (char)va_arg(args, int);
                                append(buffer, maxLen, curLen, &c, 1);
                            }
                            break;
                        case '%': // '%' 문자 자체
                            append(buffer, maxLen, curLen, "%", 1);
                            break;
                        default: // 지원하지 않는 포맷은 원문 출력
                            append(buffer, maxLen, curLen, "%", 1);
                            append(buffer, maxLen, curLen, p, 1);
                            break;
                    }
                }
                p++;
            }
            return (int)curLen;
        }

    } // string
} // cms
