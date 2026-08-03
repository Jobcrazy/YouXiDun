package com.example.ping;

import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Date;
import java.util.Deque;
import java.util.List;
import java.util.Locale;

public class LogAdapter extends RecyclerView.Adapter<LogAdapter.VH> {

    public static final int TYPE_INFO  = 0;
    public static final int TYPE_PING  = 1;
    public static final int TYPE_PONG  = 2;
    public static final int TYPE_ERROR = 3;
    public static final int TYPE_WARN  = 4;

    /** Maximum retained entries, preventing unbounded memory growth. */
    private static final int MAX_LOGS = 200;

    private static final SimpleDateFormat SDF =
            new SimpleDateFormat("HH:mm:ss.SSS", Locale.getDefault());

    private final Deque<LogEntry> deque = new ArrayDeque<>();
    // RecyclerView needs random access, so the list mirrors the deque.
    private final List<LogEntry> list = new ArrayList<>();

    static class LogEntry {
        String time;
        String text;
        int type;
        LogEntry(String time, String text, int type) {
            this.time = time;
            this.text = text;
            this.type = type;
        }
    }

    static class VH extends RecyclerView.ViewHolder {
        TextView tvTime, tvMsg;
        View indicator;
        VH(View v) {
            super(v);
            tvTime    = v.findViewById(R.id.tv_time);
            tvMsg     = v.findViewById(R.id.tv_msg);
            indicator = v.findViewById(R.id.view_indicator);
        }
    }

    /** Adds an entry and removes the oldest one after reaching the limit. */
    public void addLog(String text, int type) {
        String time = SDF.format(new Date());
        LogEntry entry = new LogEntry(time, text, type);

        if (deque.size() >= MAX_LOGS) {
            deque.pollFirst();
            list.remove(0);
            notifyItemRemoved(0);
        }

        deque.addLast(entry);
        list.add(entry);
        int pos = list.size() - 1;
        notifyItemInserted(pos);
    }

    @NonNull
    @Override
    public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View v = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_log, parent, false);
        return new VH(v);
    }

    @Override
    public void onBindViewHolder(@NonNull VH holder, int position) {
        LogEntry e = list.get(position);
        holder.tvTime.setText(e.time);
        holder.tvMsg.setText(e.text);

        int color;
        switch (e.type) {
            case TYPE_PING:  color = Color.parseColor("#4FC3F7"); break; // Blue
            case TYPE_PONG:  color = Color.parseColor("#81C784"); break; // Green
            case TYPE_ERROR: color = Color.parseColor("#EF5350"); break; // Red
            case TYPE_WARN:  color = Color.parseColor("#FFB74D"); break; // Orange
            default:         color = Color.parseColor("#B0BEC5"); break; // Gray
        }
        holder.indicator.setBackgroundColor(color);
        holder.tvMsg.setTextColor(color);
        holder.tvTime.setTextColor(Color.parseColor("#78909C"));
    }

    @Override
    public int getItemCount() {
        return list.size();
    }
}
