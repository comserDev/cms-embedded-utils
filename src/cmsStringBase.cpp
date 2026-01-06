/// @author comser.dev
///
/// 문자열 라이브러리의 공통 비즈니스 로직을 담당하는 베이스 클래스 구현부입니다.
/// 실제 버퍼를 소유하지 않고 주입된 메모리를 관리함으로써 템플릿 코드 비대화(Code Bloat)를
/// 방지하고 시스템 자원을 효율적으로 사용합니다.

#include "cmsStringBase.h" // 베이스 클래스 정의
#include "cmsUtil.h"       // 문자열 처리 헬퍼 함수

namespace cms {

// ==================================================================================================
// [StringBase] 개요
// - 왜 존재하는가: 다양한 크기의 String 템플릿 객체들이 공통 로직을 공유하여 바이너리 크기를 줄이기 위해 존재합니다.
// - 어떻게 동작하는가: 외부 버퍼 포인터와 용량 정보를 유지하며, 모든 가공은 cms::string 유틸리티를 통해 수행합니다.
// ==================================================================================================

    /// 외부 버퍼를 클래스 관리 체계에 연결하는 생성자입니다.
    ///
    /// Why: 템플릿 클래스에서 할당한 메모리를 공통 로직에서 제어하기 위함입니다.
    /// How: 전달받은 포인터와 크기를 저장하고, 기존 데이터가 있다면 길이를 동기화합니다.
    ///
    /// 사용 예:
    /// @code
    /// StringBase base(buffer, 64);
    /// @endcode
    ///
    /// @param b 문자열 데이터를 저장할 외부 char 배열 포인터
    /// @param c 버퍼의 전체 물리적 용량 (단위: bytes, 널 종료 문자 포함)
    StringBase::StringBase(char* b, size_t c)
        : _buf(b), _capacity(c), _len(0), _maxLenSeen(0) {
        if (b) {
            _len = strlen(b);
            _maxLenSeen = _len;
        }
    }

    StringBase::StringBase(char* b, size_t c, size_t l)
        : _buf(b), _capacity(c), _len(l), _maxLenSeen(l) {}

    /// 현재 버퍼의 사용량을 퍼센트(%) 단위로 계산합니다.
    ///
    /// Why: 런타임 중 버퍼 오버플로우 위험을 모니터링하기 위함입니다.
    /// How: 현재 문자열 길이를 가용 용량(capacity - 1)으로 나누어 계산합니다.
    ///
    /// 사용 예:
    /// @code
    /// float usage = s.utilization();
    /// @endcode
    ///
    /// @return 0.0 ~ 100.0 사이의 현재 사용률
    float StringBase::utilization() const {
        if (_capacity <= 1) return 0.0f;
        return (static_cast<float>(_len) / (_capacity - 1)) * 100.0f;
    }

    /// 객체 생성 이후 도달했던 최대 버퍼 사용량을 반환합니다.
    ///
    /// Why: 시스템 운영 중 버퍼 크기가 적절했는지 판단하는 프로파일링 지표로 활용합니다.
    /// How: 데이터 변경 시마다 갱신된 _maxLenSeen 값을 기준으로 계산합니다.
    ///
    /// 사용 예:
    /// @code
    /// float peak = s.peakUtilization();
    /// @endcode
    ///
    /// @return 0.0 ~ 100.0 사이의 최대 사용률 (High Water Mark)
    float StringBase::peakUtilization() const {
        if (_capacity <= 1) return 0.0f;
        return (static_cast<float>(_maxLenSeen) / (_capacity - 1)) * 100.0f;
    }

    /// 문자열을 즉시 비웁니다.
    ///
    /// Why: 기존 데이터를 무효화하고 새로운 조립을 준비하기 위함입니다.
    /// How: 첫 바이트에 '\0'을 쓰고 길이를 0으로 설정하여 O(1) 속도로 초기화합니다.
    ///
    /// 사용 예:
    /// @code
    /// s.clear();
    /// @endcode
    void StringBase::clear() {
        if (_capacity > 0) {
            _buf[0] = '\0';
            _len = 0;
        }
    }

    /// C 스타일 문자열을 대입합니다.
    ///
    /// Why: 기존 내용을 버리고 새로운 문자열로 교체하기 위함입니다.
    /// How: strlcpy를 사용하여 버퍼 오버런을 방지하며 안전하게 복사합니다.
    ///
    /// 사용 예:
    /// @code
    /// s = "New Value";
    /// @endcode
    ///
    /// @param src 복사할 원본 문자열 포인터
    ///
    /// @return 자기 자신의 참조
    StringBase& StringBase::operator=(const char* src) {
        if (src) {
            // strlcpy는 원본 문자열(src)의 전체 길이를 반환합니다.
            size_t srcLen = strlcpy(_buf, src, _capacity);

            // 실제 버퍼에 복사된 길이는 (원본 길이)와 (용량-1) 중 작은 값입니다.
            if (_capacity > 0) {
                _len = (srcLen >= _capacity) ? (_capacity - 1) : srcLen;
            } else {
                _len = 0;
            }
            updatePeak();
        }
        else clear();
        return *this;
    }

    /// 다른 StringBase 객체의 내용을 복사합니다.
    ///
    /// Why: 객체 간의 데이터를 효율적으로 전달하기 위함입니다.
    /// How: 상대방의 길이를 이미 알고 있으므로 strlen 없이 memcpy로 고속 복사합니다.
    ///
    /// 사용 예:
    /// @code
    /// s1 = s2;
    /// @endcode
    ///
    /// @param other 복사 대상 객체 참조
    ///
    /// @return 자기 자신의 참조
    StringBase& StringBase::operator=(const StringBase& other) {
        if (this != &other) {
            size_t srcLen = other.length();
            // 내 버퍼 크기에 맞춰 복사할 길이를 제한
            _len = (srcLen < _capacity - 1) ? srcLen : _capacity - 1;

            if (_len > 0) {
                memcpy(_buf, other.c_str(), _len);
            }
            _buf[_len] = '\0';
            updatePeak();
        }
        return *this;
    }

    StringBase& StringBase::operator=(const cms::string::Token& token) {
        clear();
        append(token.ptr, token.len);
        return *this;
    }

    /// 기존 문자열 뒤에 문자열을 결합합니다.
    /// @param src 추가할 문자열 포인터
    /// @return 자기 자신의 참조
    StringBase& StringBase::operator+=(const char* src) {
        if (src) append(src, strlen(src));
        return *this;
    }

    /// 기존 문자열 뒤에 단일 문자를 결합합니다.
    /// @param c 추가할 문자
    /// @return 자기 자신의 참조
    StringBase& StringBase::operator+=(char c) {
        append(&c, 1);
        return *this;
    }

    /// 기존 문자열 뒤에 다른 객체의 내용을 결합합니다.
    /// @param other 추가할 대상 객체
    /// @return 자기 자신의 참조
    StringBase& StringBase::operator+=(const StringBase& other) {
        size_t srcLen = other.length();
        if (srcLen > 0 && _capacity > 1) {
            // 남은 공간 계산 (널 종료 문자 제외)
            size_t available = (_len < _capacity - 1) ? (_capacity - 1 - _len) : 0;
            size_t toCopy = (srcLen < available) ? srcLen : available;

            if (toCopy > 0) {
                memcpy(_buf + _len, other.c_str(), toCopy);
                _len += toCopy;
                _buf[_len] = '\0';
                updatePeak();
            }
        }
        return *this;
    }

    StringBase& StringBase::operator+=(const cms::string::Token& token) {
        append(token.ptr, token.len);
        return *this;
    }

    /// 데이터를 안전하게 덧붙이는 핵심 로직입니다.
    ///
    /// Why: 버퍼 경계를 넘지 않으면서 데이터를 효율적으로 추가하기 위함입니다.
    /// How: cms::string::append 유틸리티에 위임하여 남은 용량만큼만 복사합니다.
    ///
    /// 사용 예:
    /// @code
    /// s.append("Data", 4);
    /// @endcode
    ///
    /// @param s 추가할 데이터 포인터
    /// @param len 추가할 데이터의 바이트 길이
    void StringBase::append(const char* s, size_t len) {
        if (!s || len == 0 || _capacity <= 1) return;

        // [최적화] 외부 유틸리티 호출 대신 직접 memcpy 수행 (함수 호출 오버헤드 제거)
        size_t available = (_len < _capacity - 1) ? (_capacity - 1 - _len) : 0;
        size_t toCopy = (len < available) ? len : available;

        if (toCopy > 0) {
            memcpy(_buf + _len, s, toCopy);
            _len += toCopy;
            _buf[_len] = '\0';
            updatePeak();
        }
    }

    void StringBase::append(const cms::string::Token& token) {
        append(token.ptr, token.len);
    }

    /// 문자열 양 끝의 공백을 제거합니다.
    ///
    /// Why: 사용자 입력이나 통신 데이터의 불필요한 여백을 정리하기 위함입니다.
    /// How: memmove를 사용하여 데이터를 재배치하는 In-place 수정 방식입니다.
    void StringBase::trim() {
        _len = cms::string::trim(_buf);
        updatePeak();
    }

    /// 특정 접두사로 시작하는지 확인합니다.
    /// @param prefix 찾을 접두사
    /// @param ignoreCase 대소문자 무시 여부
    /// @return 일치 여부
    bool StringBase::startsWith(const char* prefix, bool ignoreCase) const {
        if (!prefix) return false;
        size_t pLen = strlen(prefix);
        // [최적화] 접두사가 현재 문자열보다 길면 절대 일치할 수 없음
        if (pLen > _len) return false;
        return cms::string::startsWith(_buf, prefix, pLen, ignoreCase);
    }

    /// 특정 문자열의 논리적 위치를 찾습니다.
    /// @param target 찾을 문자열
    /// @param startChar 검색 시작 글자 위치
    /// @param ignoreCase 대소문자 무시 여부
    /// @return 글자 단위 인덱스 (없으면 -1)
    int StringBase::find(const char* target, size_t startChar, bool ignoreCase) const {
        if (!target) return -1;
        return cms::string::find(_buf, _len, target, strlen(target), startChar, ignoreCase);
    }

    /// 특정 문자의 논리적 위치를 찾습니다.
    int StringBase::indexOf(char c, size_t startChar, bool ignoreCase) const {
        char tmp[2] = {c, '\0'};
        return cms::string::find(_buf, _len, tmp, 1, startChar, ignoreCase);
    }

    /// 특정 문자열의 논리적 위치를 찾습니다.
    int StringBase::indexOf(const char* str, size_t startChar, bool ignoreCase) const {
        return find(str, startChar, ignoreCase);
    }

    /// 마지막으로 나타나는 문자열의 위치를 찾습니다.
    int StringBase::lastIndexOf(const char* target, bool ignoreCase) const {
        if (!target) return -1;
        return cms::string::lastIndexOf(_buf, _len, target, strlen(target), ignoreCase);
    }

    /// 마지막으로 나타나는 문자의 위치를 찾습니다.
    int StringBase::lastIndexOf(char c, bool ignoreCase) const {
        char tmp[2] = {c, '\0'};
        return cms::string::lastIndexOf(_buf, _len, tmp, 1, ignoreCase);
    }

    /// 특정 문자열 포함 여부를 확인합니다.
    bool StringBase::contains(const char* target, bool ignoreCase) const {
        if (!target) return false;
        return cms::string::contains(_buf, _len, target, strlen(target), ignoreCase);
    }

    /// 정규표현식 패턴과 일치하는지 확인합니다.
    bool StringBase::matches(const char* pattern) const {
        return cms::string::matches(_buf, pattern);
    }

    /// 특정 접미사로 끝나는지 확인합니다.
    bool StringBase::endsWith(const char* suffix, bool ignoreCase) const {
        if (!suffix) return false;
        size_t sLen = strlen(suffix);
        // [최적화] 접미사가 현재 문자열보다 길면 절대 일치할 수 없음
        if (sLen > _len) return false;
        return cms::string::endsWith(_buf, _len, suffix, sLen, ignoreCase);
    }

    /// 문자열 내의 특정 패턴을 모두 치환합니다.
    ///
    /// Why: 텍스트 가공 및 템플릿 치환을 위해 필요합니다.
    /// How: 치환 후 길이가 변할 경우 데이터를 재배치하며 버퍼 크기를 초과하면 중단됩니다.
    void StringBase::replace(const char* from, const char* to, bool ignoreCase) {
        _len = cms::string::replace(_buf, _capacity, _len, from, to, ignoreCase);
        updatePeak();
    }

    /// 정수 데이터를 텍스트로 변환하여 덧붙입니다.
    ///
    /// Why: printf의 무거운 오버헤드 없이 고속으로 숫자를 직렬화하기 위함입니다.
    /// How: 숫자를 역순으로 추출한 뒤 뒤집는 전용 알고리즘을 사용합니다.
    ///
    /// @param val 추가할 정수 값
    /// @param width 최소 출력 너비
    /// @param padChar 채움 문자
    void StringBase::appendInt(long val, int width, char padChar) {
        cms::string::appendInt(_buf, _capacity, _len, val, width, padChar);
        updatePeak();
    }

    /// 실수 데이터를 텍스트로 변환하여 덧붙입니다.
    /// @param val 추가할 실수 값
    /// @param decimalPlaces 소수점 이하 자리수
    void StringBase::appendFloat(float val, int decimalPlaces) {
        cms::string::appendFloat(_buf, _capacity, _len, val, decimalPlaces);
        updatePeak();
    }

    /// 기존 내용을 지우고 정수 값을 설정합니다.
    void StringBase::fromInt(long val) { clear(); appendInt(val, 0, ' '); }
    /// 기존 내용을 지우고 실수 값을 설정합니다.
    void StringBase::fromFloat(float val, int decimalPlaces) { clear(); appendFloat(val, decimalPlaces); }

    /// 가변 인자 리스트를 사용하여 포맷팅된 문자열을 기존 내용 뒤에 추가합니다.
    ///
    /// Why: printf 스타일의 유연한 조립 기능을 제공하기 위함입니다.
    /// How: 초경량 엔진을 사용하여 표준 vsnprintf 대비 스택 소모를 최소화합니다.
    ///
    /// @param format 포맷 문자열
    /// @param args 가변 인자 리스트
    ///
    /// @return 포맷팅 후 최종 문자열의 전체 바이트 길이
    int StringBase::appendPrintf(const char* format, va_list args) {
        _len = cms::string::appendPrintf(_buf, _capacity, _len, format, args);
        updatePeak();
        return (int)_len;
    }

    /// printf 스타일로 문자열을 조립합니다. 기존 내용은 삭제됩니다.
    ///
    /// 사용 예:
    /// @code
    /// s.printf("ID:%d, Val:%f", 1, 3.14);
    /// @endcode
    ///
    /// @param format 포맷 문자열
    ///
    /// @return 작성된 문자열의 바이트 길이
    int StringBase::printf(const char* format, ...) {
        if (!format) return 0;
        clear();
        va_list args;
        va_start(args, format);
        int ret = appendPrintf(format, args);
        va_end(args);
        return ret;
    }

    /// 스트림 스타일로 문자열을 결합합니다.
    StringBase& StringBase::operator<<(const char* s) {
        append(s, s ? strlen(s) : 0);
        return *this;
    }

    /// 스트림 스타일로 문자를 결합합니다.
    StringBase& StringBase::operator<<(char c) {
        append(&c, 1);
        return *this;
    }

    /// 스트림 스타일로 정수를 결합합니다.
    StringBase& StringBase::operator<<(int v) {
        appendInt((long)v);
        return *this;
    }

    /// 스트림 스타일로 long 정수를 결합합니다.
    StringBase& StringBase::operator<<(long v) {
        appendInt(v);
        return *this;
    }

    /// 스트림 스타일로 unsigned long 정수를 결합합니다.
    StringBase& StringBase::operator<<(unsigned long v) {
        appendInt((long)v);
        return *this;
    }

    /// 스트림 스타일로 실수를 결합합니다.
    StringBase& StringBase::operator<<(float v) {
        appendFloat(v);
        return *this;
    }

    /// 스트림 스타일로 double 실수를 결합합니다.
    StringBase& StringBase::operator<<(double v) {
        appendFloat((float)v);
        return *this;
    }

    /// 스트림 스타일로 다른 객체의 내용을 결합합니다.
    StringBase& StringBase::operator<<(const StringBase& other) {
        append(other.c_str(), other.length());
        return *this;
    }

    StringBase& StringBase::operator<<(const cms::string::Token& token) {
        append(token.ptr, token.len);
        return *this;
    }

    /// 특정 글자 위치에 문자열을 끼워 넣습니다.
    ///
    /// How: memmove를 사용하여 기존 데이터를 뒤로 밀어내고 제자리에서 수정합니다.
    /// @param charIdx 삽입할 논리적 글자 위치
    /// @param src 삽입할 문자열 포인터
    void StringBase::insert(size_t charIdx, const char* src) {
        _len = cms::string::insert(_buf, _capacity, _len, charIdx, src);
        sanitize();
    }

    /// 특정 글자 위치에 문자를 끼워 넣습니다.
    void StringBase::insert(size_t charIdx, char c) {
        char tmp[2] = {c, '\0'};
        insert(charIdx, tmp);
    }

    /// 특정 구간의 글자들을 삭제합니다.
    /// @param charIdx 삭제 시작 위치
    /// @param charCount 삭제할 글자 수
    void StringBase::remove(size_t charIdx, size_t charCount) {
        _len = cms::string::remove(_buf, _len, charIdx, charCount);
        updatePeak();
    }

    /// 문자열을 정수로 변환합니다.
    /// @return 변환된 정수 값 (실패 시 0)
    int StringBase::toInt() const { return cms::string::toInt(_buf); }

    /// 구분자를 기준으로 문자열을 분리합니다.
    ///
    /// Why: 프로토콜 패킷이나 CSV 데이터를 파싱하기 위함입니다.
    /// How: 원본 버퍼의 구분자 위치에 '\0'을 삽입하는 파괴적(Destructive) 방식을 사용합니다.
    ///
    /// @param delimiter 구분 문자
    /// @param tokens 분리된 포인터들을 저장할 배열
    /// @param maxTokens 최대 토큰 수
    ///
    /// @return 실제 분리된 토큰 개수
    size_t StringBase::split(char delimiter, char** tokens, size_t maxTokens) {
        return cms::string::split(_buf, delimiter, tokens, maxTokens);
    }

    /// 비파괴적 분할 래퍼 함수
    size_t StringBase::split(char delimiter, cms::string::Token* tokens, size_t maxTokens) const {
        return cms::string::split(_buf, delimiter, tokens, maxTokens);
    }

    /// 모든 영문을 대문자로 변환합니다.
    void StringBase::toUpperCase() { cms::string::toUpperCase(_buf); }
    /// 모든 영문을 소문자로 변환합니다.
    void StringBase::toLowerCase() { cms::string::toLowerCase(_buf); }

    /// 논리적 글자 수를 반환합니다. (UTF-8 인식)
    size_t StringBase::count() const { return cms::string::utf8_strlen(_buf); }

    /// 지정된 글자 범위를 추출하여 대상 객체에 저장합니다.
    ///
    /// How: 대상 객체(dest)의 버퍼에 결과를 직접 쓰고 상태를 업데이트합니다.
    /// @param dest 결과를 담을 객체 참조
    /// @param left 시작 글자 인덱스
    /// @param right 종료 글자 인덱스
    void StringBase::substring(StringBase& dest, size_t left, size_t right) const {
        dest.clear();
        dest._len = cms::string::substring(_buf, dest._buf, dest._capacity, left, right);
        dest.updatePeak();
    }
    /// 물리적 바이트 오프셋 기준으로 부분 문자열을 추출합니다.
    void StringBase::byteSubstring(StringBase& dest, size_t startByte, size_t endByte) const {
        dest.clear();
        cms::string::byteSubstring(_buf, dest._buf, dest._capacity, startByte, endByte);
        dest.sanitize(); // sanitize() 내부에서 updateLength 호출
    }

    /// 유효한 UTF-8 인코딩인지 확인합니다.
    bool StringBase::isValid() const { return cms::string::validateUtf8(_buf); }

    /// 버퍼 끝에서 잘린 멀티바이트 문자를 정제합니다.
    ///
    /// Why: 통신이나 치환 과정에서 한글 바이트가 잘려 깨진 기호가 출력되는 것을 방지합니다.
    void StringBase::sanitize() {
        _len = cms::string::sanitizeUtf8(_buf, _capacity);
        updatePeak();
    }

    /// 문자열 내용의 일치 여부를 확인합니다.
    ///
    /// Why: 포인터 주소가 아닌 실제 데이터의 동등성을 비교하기 위함입니다.
    /// @param other 비교 대상 문자열
    /// @param ignoreCase 대소문자 무시 여부
    bool StringBase::equals(const char* other, bool ignoreCase) const {
        if (!other) return isEmpty();
        return cms::string::equals(_buf, _len, other, strlen(other), ignoreCase);
    }

    void StringBase::updateLength() {
        if (_buf) {
            _len = strlen(_buf);
            updatePeak();
        } else {
            _len = 0;
        }
    }

    /// 최대 사용량 지표를 갱신합니다.
    void StringBase::updatePeak() {
        if (_len > _maxLenSeen) _maxLenSeen = _len;
    }

    /// 외부 문자열과 객체의 비교 연산자입니다.
    bool operator==(const char* lhs, const StringBase& rhs) {
        if (!lhs) return rhs.isEmpty();
        size_t lLen = strlen(lhs);
        return cms::string::equals(lhs, lLen, rhs._buf, rhs._len, false);
    }

    /// 외부 문자열과 객체의 불일치 연산자입니다.
    bool operator!=(const char* lhs, const StringBase& rhs) {
        return !(lhs == rhs);
    }
}
