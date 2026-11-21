package com.tcl.bi.service;

interface IBiReportService {
    void reportData(String data);
    void sendData(String data);
    int report(String data);
}
