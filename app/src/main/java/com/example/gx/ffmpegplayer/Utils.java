package com.example.gx.ffmpegplayer;


public class Utils {
    public static byte[] toBytes( char[] chars)
    {
        byte[]	bytes = new byte[chars.length];
        for (int i = 0, j = chars.length; i < j; i++) {
            bytes[i] = ( byte ) ( chars[i] );
        }
        return bytes;
    }

    public static char[] toChars( byte[] bytes) {
        char[]	chars = new char[bytes.length];
        for (int i = 0, j = chars.length; i < j; i++) {
            chars[i] = (char)(bytes[i] & 0xff);
        }
        return chars;
    }
}
