package com.example.ping;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import android.widget.Button;
import android.widget.EditText;

import com.udptcp.netguard.Shield;

import java.util.Locale;

public class MainActivity extends AppCompatActivity implements TcpClient.Listener {

    private volatile TcpClient tcpClient;
    private LogAdapter logAdapter;
    private RecyclerView recyclerView;
    private TextView tvStatus;
    private Button btnConnect, btnDisconnect;
    private EditText etHost, etPort;
    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private boolean destroying = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvStatus       = findViewById(R.id.tv_status);
        btnConnect     = findViewById(R.id.btn_connect);
        btnDisconnect  = findViewById(R.id.btn_disconnect);
        etHost         = findViewById(R.id.et_host);
        etPort         = findViewById(R.id.et_port);
        recyclerView   = findViewById(R.id.rv_logs);

        logAdapter = new LogAdapter();
        LinearLayoutManager layoutManager = new LinearLayoutManager(this);
        layoutManager.setStackFromEnd(true);
        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(logAdapter);

        btnConnect.setOnClickListener(v -> {
            String host    = etHost.getText().toString().trim();
            String portStr = etPort.getText().toString().trim();
            if (host.isEmpty() || portStr.isEmpty()) {
                appendLog("⚠ Enter a host and port", LogAdapter.TYPE_WARN);
                return;
            }
            int port;
            try {
                port = Integer.parseInt(portStr);
            } catch (NumberFormatException e) {
                appendLog("⚠ Invalid port", LogAdapter.TYPE_WARN);
                return;
            }
            if (port < 1 || port > 65535) {
                appendLog("⚠ Port must be between 1 and 65535", LogAdapter.TYPE_WARN);
                return;
            }
            connect(host, port);
        });

        btnDisconnect.setOnClickListener(v -> disconnect());
        applyDisconnectedState("Not connected");

        int iRet = Shield.Init(null, "ac7f95bb-1e4d-4186-8e01-e6334462a608");
        if (iRet == 0) appendLog("SDK initialized successfully", LogAdapter.TYPE_INFO);
        else appendLog(String.format(
                Locale.US, "SDK initialization failed: %d", iRet), LogAdapter.TYPE_ERROR);
    }

    // ──────────────────────────────────────────────
    //  Connect and disconnect.
    // ──────────────────────────────────────────────

    private void connect(String host, int port) {
        if (tcpClient != null) tcpClient.stop();
        btnConnect.setVisibility(View.GONE);
        btnDisconnect.setVisibility(View.VISIBLE);
        setStatus("Connecting...", false);
        appendLog("→ Connecting to " + host + ":" + port, LogAdapter.TYPE_INFO);

        tcpClient = new TcpClient(host, port, this);
        tcpClient.start();
    }

    // MainActivity.java
    private void disconnect() {
        if (tcpClient != null) {
            tcpClient.stop();
            tcpClient = null;
        }
        // A manual disconnect updates the UI without waiting for a callback.
        applyDisconnectedState("Not connected");
        appendLog("• Disconnected manually", LogAdapter.TYPE_INFO);
    }

    // ──────────────────────────────────────────────
    //  UI state transitions. These methods must run on the main thread.
    // ──────────────────────────────────────────────

    private void applyConnectedState() {
        btnConnect.setVisibility(View.GONE);
        btnDisconnect.setVisibility(View.VISIBLE);
        setStatus("Connected", true);
    }

    private void applyDisconnectedState(String statusText) {
        btnConnect.setVisibility(View.VISIBLE);
        btnDisconnect.setVisibility(View.GONE);
        setStatus(statusText, false);
    }

    private void setStatus(String text, boolean connected) {
        tvStatus.setText(text);
        tvStatus.setTextColor(ContextCompat.getColor(this,
                connected ? R.color.status_ok : R.color.status_off));
    }

    private void appendLog(String msg, int type) {
        uiHandler.post(() -> {
            if (destroying) return;
            logAdapter.addLog(msg, type);
            recyclerView.scrollToPosition(logAdapter.getItemCount() - 1);
        });
    }

    // ──────────────────────────────────────────────
    //  TcpClient.Listener callbacks originate on worker threads.
    // ──────────────────────────────────────────────

    @Override
    public void onConnected() {
        uiHandler.post(this::applyConnectedState);
        appendLog("✓ Connected successfully", LogAdapter.TYPE_INFO);
    }

    @Override
    public void onDisconnected(String reason) {
        tcpClient = null;
        uiHandler.post(() -> applyDisconnectedState("Disconnected"));
        appendLog("✗ Disconnected: " + reason, LogAdapter.TYPE_ERROR);
    }

    @Override
    public void onSent(String json) {
        appendLog("↑ " + json, LogAdapter.TYPE_PING);
    }

    @Override
    public void onReceived(String json) {
        appendLog("↓ " + json, LogAdapter.TYPE_PONG);
    }

    @Override
    public void onError(String err) {
        tcpClient = null;
        uiHandler.post(() -> applyDisconnectedState("Connection failed"));
        appendLog("✗ " + err, LogAdapter.TYPE_ERROR);
    }

    // ──────────────────────────────────────────────
    //  Lifecycle.
    // ──────────────────────────────────────────────

    @Override
    protected void onDestroy() {
        destroying = true;
        if (tcpClient != null) {
            tcpClient.stop();
            tcpClient = null;
        }
        uiHandler.removeCallbacksAndMessages(null);
        super.onDestroy();
    }
}
