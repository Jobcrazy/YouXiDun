package com.example.ping;

import org.json.JSONObject;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

/**
 * TCP client.
 * Protocol: a 4-byte little-endian length header followed by the payload.
 * Sends one ping per second and receives pong messages on a background thread.
 */
public class TcpClient {

    private static final int BUFF_SIZE = 16 * 1024;
    private static final int CONNECT_TIMEOUT_MS = 5_000;

    public interface Listener {
        void onConnected();
        void onDisconnected(String reason);
        void onSent(String json);
        void onReceived(String json);
        void onError(String err);
    }

    private final String host;
    private final int port;
    private final Listener listener;

    private volatile Socket socket;
    private volatile OutputStream out;
    private volatile InputStream in;

    private final AtomicBoolean running = new AtomicBoolean(false);
    private final AtomicBoolean terminated = new AtomicBoolean(false);
    private final AtomicLong idx = new AtomicLong(0);
    private final ExecutorService executor = Executors.newFixedThreadPool(2);

    private static final SimpleDateFormat SDF =
            new SimpleDateFormat("HH:mm:ss.SSS", Locale.getDefault());

    public TcpClient(String host, int port, Listener listener) {
        this.host = host;
        this.port = port;
        this.listener = listener;
    }

    public void start() {
        executor.execute(this::connectAndRun);
    }

    public void stop() {
        terminate(null, false, false);
    }

    // ──────────────────────────────────────────────
    //  Connect and start the send and receive workers.
    // ──────────────────────────────────────────────

    private void connectAndRun() {
        try {
            Socket client = new Socket();
            socket = client;
            client.connect(new InetSocketAddress(host, port), CONNECT_TIMEOUT_MS);
            if (terminated.get()) {
                closeSocket();
                return;
            }
            out = socket.getOutputStream();
            in = socket.getInputStream();
            if (terminated.get()) {
                closeSocket();
                return;
            }
            running.set(true);
            listener.onConnected();

            // Start the receive worker.
            executor.execute(this::recvLoop);
            // Use the current worker for sending.
            sendLoop();
        } catch (IOException e) {
            terminate("Connection failed: " + errorMessage(e), true, false);
        }
    }

    // ──────────────────────────────────────────────
    //  Ping send loop.
    // ──────────────────────────────────────────────

    private void sendLoop() {
        while (running.get()) {
            try {
                long i = idx.getAndIncrement();
                JSONObject obj = new JSONObject();
                obj.put("cmd", "ping");
                obj.put("id", i);
                obj.put("time", SDF.format(new Date()));
                String json = obj.toString();

                write(json.getBytes(StandardCharsets.UTF_8));
                listener.onSent(json);
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                break;
            } catch (Exception e) {
                terminate("Send failed: " + errorMessage(e), true, false);
                break;
            }
        }
    }

    // ──────────────────────────────────────────────
    //  Pong receive loop.
    // ──────────────────────────────────────────────

    private void recvLoop() {
        byte[] lenBuf = new byte[4];
        while (running.get()) {
            try {
                readFull(in, lenBuf, 4);
                int msgLen = ByteBuffer.wrap(lenBuf).order(ByteOrder.LITTLE_ENDIAN).getInt();
                if (msgLen <= 0 || msgLen + 4 > BUFF_SIZE) {
                    terminate("Invalid packet length: " + msgLen, true, false);
                    break;
                }
                byte[] payload = new byte[msgLen];
                readFull(in, payload, msgLen);
                String json = new String(payload, StandardCharsets.UTF_8);
                listener.onReceived(json);
            } catch (Exception e) {
                terminate(errorMessage(e), false, true);
                break;
            }
        }
    }

    // ──────────────────────────────────────────────
    //  Write a 4-byte little-endian length followed by the payload.
    // ──────────────────────────────────────────────

    private synchronized void write(byte[] data) throws IOException {
        if (data.length + 4 > BUFF_SIZE) {
            throw new IOException("Packet is too large");
        }

        byte[] frame = new byte[4 + data.length];
        ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN).putInt(data.length);
        System.arraycopy(data, 0, frame, 4, data.length);
        out.write(frame);
        out.flush();
    }

    // ──────────────────────────────────────────────
    //  Utilities.
    // ──────────────────────────────────────────────

    private static void readFull(InputStream is, byte[] buf, int len) throws IOException {
        int read = 0;
        while (read < len) {
            int n = is.read(buf, read, len - read);
            if (n < 0) throw new IOException("Connection closed");
            read += n;
        }
    }

    /** Stops all workers and emits at most one terminal callback. */
    private void terminate(String reason, boolean error, boolean disconnected) {
        if (!terminated.compareAndSet(false, true)) return;
        running.set(false);
        closeSocket();
        executor.shutdownNow();
        if (reason == null) return;
        if (error) listener.onError(reason);
        else if (disconnected) listener.onDisconnected(reason);
    }

    private static String errorMessage(Exception error) {
        String message = error.getMessage();
        return message == null || message.isEmpty()
                ? error.getClass().getSimpleName()
                : message;
    }

    private void closeSocket() {
        try {
            if (socket != null && !socket.isClosed()) socket.close();
        } catch (IOException ignored) {}
    }
}
