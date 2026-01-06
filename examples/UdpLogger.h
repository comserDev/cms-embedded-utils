/// @author comser.dev
/// @brief [Example] UDP 네트워크로 로그를 전송하는 비동기 로거 확장 예시

#pragma once

#include "../src/cmsAsyncLogger.h"
#include <WiFi.h>          // ESP32 WiFi
#include <WiFiUdp.h>       // ESP32 UDP

namespace cms {

    /// @brief AsyncLogger를 상속받아 UDP 전송 기능을 추가한 커스텀 로거
    template <uint16_t MSG_SIZE = 256, uint8_t QUEUE_DEPTH = 16>
    class UdpLogger : public AsyncLogger<MSG_SIZE, QUEUE_DEPTH> {
    public:
        /// @param ip UDP 서버 IP 주소
        /// @param port UDP 서버 포트 번호
        UdpLogger(IPAddress ip, uint16_t port) : _ip(ip), _port(port) {
            _udp.begin(UDP_LOCAL_PORT); // 임의의 로컬 포트 사용
        }

        ~UdpLogger() override {
            _udp.stop();
        }

    protected:
        /// @brief 큐에서 꺼내진 로그 메시지를 UDP 패킷으로 전송하도록 재정의합니다.
        void outputLog(const cms::StringBase& msg) override {
            _udp.beginPacket(_ip, _port);
            _udp.write(msg.c_str());
            _udp.endPacket();
        }

    private:
        IPAddress _ip;               /// UDP 서버 IP 주소
        uint16_t _port;              /// UDP 서버 포트 번호
        WiFiUDP _udp;               /// UDP 통신 객체

        static const int UDP_LOCAL_PORT = 40000;
    };

} // namespace cms
