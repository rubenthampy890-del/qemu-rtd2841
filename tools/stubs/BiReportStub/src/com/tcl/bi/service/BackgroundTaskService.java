package com.tcl.bi.service;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/**
 * No-op stub for com.tcl.bi.service.BackgroundTaskService.
 */
public class BackgroundTaskService extends Service {
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
