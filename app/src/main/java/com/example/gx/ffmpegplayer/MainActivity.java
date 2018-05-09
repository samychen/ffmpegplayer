package com.example.gx.ffmpegplayer;

import android.content.Intent;
import android.media.MediaMetadataRetriever;
import android.media.MediaPlayer;
import android.opengl.GLSurfaceView;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.VideoView;

import com.ufotosoft.bzmedia.BZMedia;
import com.ufotosoft.bzmedia.bean.VideoTransCodeParams;
import com.ufotosoft.bzmedia.utils.BZLogUtil;
import com.ufotosoft.bzmedia.widget.BZVideoView;
import com.ufotosoft.voice.soundutil.JNISoundTouch;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.security.auth.login.LoginException;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("bzffmpeg");
        System.loadLibrary("bzffmpegcmd");
        System.loadLibrary("native-lib");
    }

    private BZVideoView bz_video_view;
    private String TAG = "MainActivity";
    private String videoPath;
    private String reverseFile = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Filter/test_reverse.mp4";
    private String outputFile = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Filter/test_output.mp4";
    List<Long> ptsList = new ArrayList<>();
    private int framerate;
    List<Short> bufferlist = new ArrayList<>();
    public short[] arraycpy(short[] a,short[] b){
        short[] c = new short[a.length + b.length];
        System.arraycopy(a, 0, c, 0, a.length);
        System.arraycopy(b, 0, c, a.length, b.length);
        return c;
    }
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Intent intent = getIntent();
        //note:必须先设置pitch，然后设置其他值
        videoPath = "/storage/emulated/0/Filter/test1.mp4";//intent.getStringExtra("filepath");
        bz_video_view = (BZVideoView) findViewById(R.id.bzview);
//        bz_video_view.setVisibility(View.GONE);
        final boolean[] args = {true,true,true,true,true,true};
//        memorybuffer("/storage/emulated/0/test.pcm","/storage/emulated/0/out.pcm",args);
//        initSox2();
//        pcmtest("/storage/emulated/0/test.pcm","/storage/emulated/0/out.pcm");
        audioEffect("/storage/emulated/0/test.pcm","/storage/emulated/0/out.pcm",args);
//        JNISoundTouch.getInstance().initSpeex();
//        bz_video_view.setOnDecodeDateAvailableListener(new BZVideoView.OnDecodeDateAvailableListener() {
//            @Override
//            public void onYUVDataAvailable(byte[] bytes, int i, int i1) {
//            }
//            @Override
//            public byte[] onPCMDataAvailable(final byte[] bytes) {
////                short[] tempshort = com.ufotosoft.voice.soundutil.Utils.getShort(bytes);
////                for (int i = 0; i < tempshort.length; i++) {
////                    bufferlist.add(tempshort[i]);
////                }
////                if (bufferlist.size()>=102400){
////                    Object[] objects = bufferlist.toArray();
////                    short[] shortbuf = new short[objects.length];
////                    for (int i = 0; i < objects.length; i++) {
////                        shortbuf[i] = Short.parseShort(objects[i].toString());
////                    }
////                    short[] outbuffer = pcmbuffer(shortbuf,args);
////                    bufferlist.clear();
////                    Log.e(TAG, "send 102400 short" );
////                }
////                char[] chars = Utils.toChars(bytes);
////                char[] chars1 = memorybuffer2(chars,args);
////                return Utils.toBytes(chars1);
//                short[] aShort = pcmbuffer(com.ufotosoft.voice.soundutil.Utils.getShort(bytes),args);
//                JNISoundTouch.getInstance().speexDenose(aShort);
//                return  com.ufotosoft.voice.soundutil.Utils.shortToByteSmall(aShort);//Utils.toBytes(outchars);
////                return bytes;
//            }
//        });
        bz_video_view.setDataSource(videoPath);
        bz_video_view.setPlayLoop(false);
        bz_video_view.start();
    }
    public native void initSox();
    public native void initSox2();
    public native void closeSox();
    public native void closeSox2();
    public native void pcmtest(String input,String output);
    native char[] memorybuffer2(char[] bytes, boolean[] args);
    native short[] pcmbuffer(short[] bytes, boolean[] args);
    native void memorybuffer(String input, String output,boolean[] args);
    public native int reverse(String filePath);
    public native int reverse2(String filePath);

    public native long[] getPts();
    public native int[] getKeyframeIndex();
    public native void audioEffect(String input,String output,boolean[] args);
    public native void test(String inputFile,String decodeFile);
    public native void testseek(String inputFile,String decodeFile,long starttime,long endtime);
    public native void decode(String inputFile,String decodeFile);
    public native void saveReverseFile(String inputFile,String reverseFile,long[] pts);
    public native void saveReverseFile2(String inputFile,String reverseFile,long[] pts);
//    public native void  saveFile();
    public native void transcode(String input,String output);
    public native int reverseNative(String file_src, String file_dest,
                                    long positionUsStart, long positionUsEnd,
                                    int videoStreamNo,
                                    int audioStreamNo, int subtitleStreamNo);
    public void transcode(View v){
        new Thread(new Runnable() {
            @Override
            public void run() {
//                test(videoPath,reverseFile);
                testseek(videoPath,reverseFile,251702,351702);
            }
        }).start();

    }

    public void saveSuccessCallback(){
        Toast.makeText(MainActivity.this,"保存成功",Toast.LENGTH_SHORT).show();
    }
    public void reverse(View view) {
        bz_video_view.pause();
        framerate=reverse2(videoPath);
        doReverse2();
    }
    public static List<Long> getList(long[] arr){
        List<Long>list=new ArrayList<Long>();
        for(int i=0;i<arr.length;i++){
            list.add(arr[i]);
        }
        return list;
    }
    public void doReverse2() {
        final long[] ptsArr = getPts();
        ptsList.addAll(getList(ptsArr));
        int[] keyframeIndex = getKeyframeIndex();
        for (int i = 0; i < ptsArr.length; i++) {
            Log.e(TAG, "ptsArr: "+ptsArr[i] );
        }
        for (int i = 0; i < keyframeIndex.length; i++) {
            Log.e(TAG, "keyframeIndex: "+keyframeIndex[i] );
        }
        for (int i = 0; i < keyframeIndex.length-1; i++) {
            sort(keyframeIndex[i],keyframeIndex[i+1]-1);
        }
        sort(keyframeIndex[keyframeIndex.length-1],ptsList.size()-1);
        for (int i = 0; i < ptsList.size(); i++) {
            Log.e(TAG, "ptsList: "+ptsList.get(i) );
        }
        bz_video_view.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        new Thread(new Runnable() {
            @Override
            public void run() {
                for (int i = ptsList.size() - 1; i >= 0; i--) {
                    long cur = System.currentTimeMillis();
                    Log.e(TAG, "cur: " + cur);
                    final long pts = ptsList.get(i);
                    try {
                        Thread.sleep(framerate);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    bz_video_view.seek(pts, pts);
                    Log.e(TAG, "next: " + System.currentTimeMillis());
                }
            }
        }).start();
    }

    private void sort(int keyframeIndex, int keyframeIndex1) {
        int step = keyframeIndex1 - keyframeIndex;
        for (int i = 0; i < step/2; i++) {
            long a = ptsList.get(keyframeIndex+i);
            long b = ptsList.get(keyframeIndex1-i);
            ptsList.set(keyframeIndex+i,b);
            ptsList.set(keyframeIndex1-i,a);
        }
    }

    public void doReverse() {
        final long[] ptsArr = getPts();
        ptsList.addAll(getList(ptsArr));
//        bz_video_view.setPlayLoop(false);
//        final VideoView videoView = findViewById(R.id.videoview);
//        videoView.setVideoPath("/storage/emulated/0/Filter/test.mp4");
//        videoView.requestFocus();
//        videoView.setOnPreparedListener(new MediaPlayer.OnPreparedListener() {
//            @Override
//            public void onPrepared(MediaPlayer mediaPlayer) {
//                mediaPlayer.setOnSeekCompleteListener(new MediaPlayer.OnSeekCompleteListener() {
//                    @Override
//                    public void onSeekComplete(MediaPlayer mp) {
//                        videoView.start();
//                    }
//                });
//            }
//        });
        bz_video_view.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        new Thread(new Runnable() {
            @Override
            public void run() {
                for (int i = ptsArr.length - 1; i >= 0; i--) {
                    long cur = System.currentTimeMillis();
                    Log.e(TAG, "cur: " + cur);
                    final long pts = ptsArr[i];
                    try {
                        Thread.sleep(framerate);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    bz_video_view.seek(pts, pts);
                    Log.e(TAG, "next: " + System.currentTimeMillis());
                }
            }
        }).start();
//        bz_video_view.queueEvent(new Runnable() {
//            @Override
//            public void run() {
//                for (int i = ptsArr.length - 1; i >= 0; i--) {
//                    Log.e(TAG, "pts=: "+ ptsArr[i]);
//                    bz_video_view.seek(ptsArr[i],ptsArr[i]);
//                }
//            }
//        });
//        bz_video_view.start();

    }

    @Override
    protected void onPause() {
        if (null != bz_video_view){
            bz_video_view.pause();
            bz_video_view.onPause();
        }
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (null != bz_video_view)
            bz_video_view.onResume();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        closeSox();
        if (null != bz_video_view)
            bz_video_view.release();
        //销毁JNI环境

    }

//    public void saveFile(View view) {
//        final long[] pts = new long[ptsList.size()];
//        for (int i = 0,size = ptsList.size(); i < size; i++) {
//            pts[i] = ptsList.get(i);
//        }
//        new Thread(new Runnable() {
//            @Override
//            public void run() {
//                saveReverseFile2(videoPath,reverseFile,pts);
//            }
//        }).start();
//    }
    public void saveFile(final View view) {
        bz_video_view.pause();
        view.setEnabled(false);
        Utils.showLoadingDialog(MainActivity.this,"正在保存视频",true);
        new Thread(new Runnable() {
            @Override
            public void run() {
                long startTime = System.currentTimeMillis();
                String inputPath = videoPath;
                final String outputPath = "/storage/emulated/0/Filter/test2.mp4";
                VideoTransCodeParams videoTransCodeParams = new VideoTransCodeParams();
                videoTransCodeParams.setInputPath(inputPath);
                videoTransCodeParams.setOutputPath(outputPath);
                videoTransCodeParams.setDoWithVideo(false);
                videoTransCodeParams.setDoWithAudio(true);

                BZMedia.setOnVideoTransCodePcmCallBackListener(new BZMedia.OnVideoTransCodePcmCallBackListener() {
                    @Override
                    public byte[] onPcmCallBack(byte[] bytes) {
                        long startTime = System.currentTimeMillis();
                        short[] aShort = com.ufotosoft.voice.soundutil.Utils.getShort(bytes);
                        JNISoundTouch.getInstance().speexDenose(aShort);
                        if (aShort != null) {
                            JNISoundTouch.getInstance().putSamples(aShort, aShort.length);
                            Log.e(TAG, "onPCMDataAvailable: " + aShort.length);
                            short[] buffer = JNISoundTouch.getInstance().receiveSamples();
                            byte[] toByteSmall = com.ufotosoft.voice.soundutil.Utils.shortToByteSmall(buffer);
                            BZLogUtil.v(TAG, "处理后: " + (System.currentTimeMillis() - startTime));
                            return toByteSmall;
                        }
                        return bytes;
                    }
                });

                BZMedia.startVideoTransCode(videoTransCodeParams, new BZMedia.OnVideoTransCodeProgressListener() {
                    @Override
                    public void videoTransCodeProgress(float progress) {
                        BZLogUtil.d(TAG, "videoTransCodeProgress progress=" + progress);
                    }

                    @Override
                    public void videoTransCodeFail() {

                    }

                    @Override
                    public void videoTransCodeSuccess() {
                    }
                });
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Utils.closeLoadingDialog();
                        Toast.makeText(MainActivity.this, outputPath, Toast.LENGTH_SHORT).show();
                        bz_video_view.onResume();
                    }
                });
                BZLogUtil.d(TAG, "videoTransCode 耗时=" + (System.currentTimeMillis() - startTime));
            }
        }).start();

    }
    BZMedia.OnMergeProgressListener mergeProgressListener = new BZMedia.OnMergeProgressListener() {
        @Override
        public void mergeProgress(float v) {
            Log.e(TAG, "mergeProgress() called with: v = [" + v + "]");
        }

        @Override
        public void mergeFail() {
            Log.e(TAG, "mergeFail: " );
        }

        @Override
        public void mergeSuccess() {
            Toast.makeText(MainActivity.this,"合并成功"+outputFile,Toast.LENGTH_SHORT).show();
            Log.e(TAG, "mergeSuccess: " );
        }
    };
    public void mergeFile(View view) {
        String[] fileList = {videoPath,reverseFile};
        BZMedia.mergeVideo(fileList,outputFile,mergeProgressListener);
    }

}
