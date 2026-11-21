package com.tcl.bi.service;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;

import com.tcl.ttvs.bi.IBIService;

/**
 * No-op AIDL Binder Stub Service for com.tcl.bi.service.BiReportService.
 *
 * Prevents NullPointerException (NPE) and ServiceNotFound crashes in retained
 * system components (TvSettings, InitSetup, Factory apps) following Tier 1
 * debloating of TCL_ALL_BI.
 */
public class BiReportService extends Service {
    private static final String TAG = "BiReportServiceStub";
    private static final String DESCRIPTOR_BI = "com.tcl.bi.service.IBiReportService";
    private static final String DESCRIPTOR_TTVS = "com.tcl.ttvs.bi.IBIService";

    private final StubBinder mBinder = new StubBinder();

    public static class StubBinder extends Binder implements IBiReportService, IBIService {
        public StubBinder() {
            attachInterface(this, DESCRIPTOR_BI);
        }

        @Override
        public IBinder asBinder() {
            return this;
        }

        @Override
        public void reportData(String data) {
            // No-op: swallow all telemetry data
        }

        @Override
        public void sendData(String data) {
            // No-op
        }

        @Override
        public int report(String data) {
            return 0; // Success
        }

        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
            if (code == INTERFACE_TRANSACTION) {
                if (reply != null) {
                    reply.writeString(DESCRIPTOR_BI);
                }
                return true;
            }
            if (reply != null) {
                reply.writeNoException();
                if (code == 3) {
                    reply.writeInt(0); // integer return for report()
                }
            }
            return true;
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "BiReportService no-op stub created");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Swallow startService intents from DataReport.sendDataFromIntentBIService
        if (intent != null) {
            String reportData = intent.getStringExtra("reportData");
            if (reportData != null) {
                Log.d(TAG, "Swallowed reportData intent payload (" + reportData.length() + " bytes)");
            }
        }
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.i(TAG, "BiReportService onBind caller bound to no-op AIDL stub");
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        return false;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "BiReportService no-op stub destroyed");
    }
}
