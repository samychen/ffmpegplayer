package com.example.gx.ffmpegplayer;

import android.app.Application;

import com.ufotosoft.bzmedia.BZMedia;

/**
 * Created by 000 on 2018/4/13.
 */

public class MyApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        BZMedia.init(getApplicationContext(), BuildConfig.DEBUG);
    }
}
