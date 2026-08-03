import Foundation
import Network

/// iOS TCP client: 4-byte little-endian length header followed by UTF-8 payload.
public final class TcpNative {
    private static let queue = DispatchQueue(label: "uniapp.tcp.client")
    private static var connection: NWConnection?
    private static var timer: DispatchSourceTimer?
    private static var callback: ((String, String) -> Void)?
    private static var sequence: Int64 = 0
    private static var generation: UInt64 = 0
    private static var terminalEventSent = false
    private static let maxPacketSize = 16 * 1024

    public static func start(_ host: String, _ port: Int,
                             _ callback: @escaping (String, String) -> Void) {
        queue.async {
            stopLocked()
            guard (1...65535).contains(port),
                  let endpointPort = NWEndpoint.Port(rawValue: UInt16(port)) else {
                DispatchQueue.main.async {
                    callback("error", "非法端口: \(port)")
                }
                return
            }
            generation &+= 1
            let token = generation
            self.callback = callback
            terminalEventSent = false
            sequence = 0

            let client = NWConnection(host: NWEndpoint.Host(host), port: endpointPort, using: .tcp)
            connection = client
            client.stateUpdateHandler = { state in
                queue.async {
                    guard token == generation else { return }
                    switch state {
                    case .ready:
                        emit("connected", "")
                        receiveHeader(client, token)
                        startPingTimer(client, token)
                    case .failed(let error):
                        finish(error.localizedDescription)
                    case .waiting(let error):
                        // This demo connects to a service on the same device, so it
                        // fails immediately instead of waiting for network recovery.
                        finish(error.localizedDescription)
                    case .cancelled:
                        break
                    default:
                        break
                    }
                }
            }
            client.start(queue: queue)
        }
    }

    public static func stop() {
        queue.async { stopLocked() }
    }

    private static func startPingTimer(_ client: NWConnection, _ token: UInt64) {
        timer?.cancel()
        let source = DispatchSource.makeTimerSource(queue: queue)
        source.schedule(deadline: .now(), repeating: 1.0)
        source.setEventHandler {
            guard token == generation else { return }
            let formatter = DateFormatter()
            formatter.dateFormat = "HH:mm:ss.SSS"
            let object: [String: Any] = ["cmd": "ping", "id": sequence,
                                         "time": formatter.string(from: Date())]
            sequence += 1
            do {
                let payload = try JSONSerialization.data(withJSONObject: object)
                try send(payload, through: client, token: token)
                if let text = String(data: payload, encoding: .utf8) { emit("sent", text) }
            } catch {
                emit("error", "发送失败: \(error.localizedDescription)")
                finish(error.localizedDescription)
            }
        }
        timer = source
        source.resume()
    }

    private static func send(_ payload: Data, through client: NWConnection,
                             token: UInt64) throws {
        guard payload.count + 4 <= maxPacketSize else { throw TcpError.packetTooLarge }
        var length = UInt32(payload.count).littleEndian
        var frame = Data(bytes: &length, count: 4)
        frame.append(payload)
        client.send(content: frame, completion: .contentProcessed { error in
            guard token == generation, let error = error else { return }
            queue.async {
                emit("error", "发送失败: \(error.localizedDescription)")
                finish(error.localizedDescription)
            }
        })
    }

    private static func receiveHeader(_ client: NWConnection, _ token: UInt64) {
        receiveExactly(4, client, token) { data, error in
            if let error = error { finish(error.localizedDescription); return }
            guard let data = data, data.count == 4 else { finish("连接已关闭"); return }
            let length = data.withUnsafeBytes { raw -> Int in
                var value: UInt32 = 0
                memcpy(&value, raw.baseAddress!, 4)
                return Int(UInt32(littleEndian: value))
            }
            guard length > 0, length <= maxPacketSize else {
                finish("非法包长度: \(length)"); return
            }
            receiveExactly(length, client, token) { payload, error in
                if let error = error { finish(error.localizedDescription); return }
                guard let payload = payload,
                      let text = String(data: payload, encoding: .utf8) else {
                    finish("收到的数据不是有效 UTF-8"); return
                }
                emit("received", text)
                receiveHeader(client, token)
            }
        }
    }

    private static func receiveExactly(_ length: Int, _ client: NWConnection,
                                       _ token: UInt64,
                                       _ completion: @escaping (Data?, Error?) -> Void) {
        var buffer = Data()
        func next() {
            client.receive(minimumIncompleteLength: 1,
                           maximumLength: length - buffer.count) { data, _, complete, error in
                queue.async {
                    guard token == generation else { return }
                    if let data = data { buffer.append(data) }
                    if let error = error { completion(nil, error) }
                    else if buffer.count == length { completion(buffer, nil) }
                    else if complete { completion(nil, TcpError.connectionClosed) }
                    else { next() }
                }
            }
        }
        next()
    }

    private static func finish(_ reason: String) {
        guard !terminalEventSent else { return }
        terminalEventSent = true
        emit("disconnected", reason)
        stopLocked()
    }

    private static func emit(_ type: String, _ message: String) {
        let handler = callback
        DispatchQueue.main.async { handler?(type, message) }
    }

    private static func stopLocked() {
        generation &+= 1
        timer?.cancel()
        timer = nil
        connection?.stateUpdateHandler = nil
        connection?.cancel()
        connection = nil
        callback = nil
    }

    private enum TcpError: LocalizedError {
        case packetTooLarge, connectionClosed
        var errorDescription: String? {
            switch self {
            case .packetTooLarge: return "数据包过大"
            case .connectionClosed: return "连接已关闭"
            }
        }
    }
}
