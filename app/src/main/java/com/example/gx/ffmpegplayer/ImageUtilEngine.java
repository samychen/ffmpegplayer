package com.example.gx.ffmpegplayer;

/**
 * Created by 000 on 2018/4/12.
 */

public class ImageUtilEngine {
    public native int[] decodeYUV420SP(byte[] buf, int width, int heigth);
}
