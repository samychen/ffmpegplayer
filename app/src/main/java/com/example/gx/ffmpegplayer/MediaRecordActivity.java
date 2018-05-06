package com.example.gx.ffmpegplayer;

import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.Bundle;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;

import com.ufotosoft.bzmedia.BZMedia;
import com.ufotosoft.bzmedia.bean.FilterInfo;
import com.ufotosoft.bzmedia.bean.VideoSize;
import com.ufotosoft.bzmedia.bean.Viewport;
import com.ufotosoft.bzmedia.recorder.OnRecorderErrorListener;
import com.ufotosoft.bzmedia.recorder.VideoRecorderBase;
import com.ufotosoft.bzmedia.recorder.VideoRecorderBuild;
import com.ufotosoft.bzmedia.utils.BZFileUtils;
import com.ufotosoft.bzmedia.utils.BZLogUtil;
import com.ufotosoft.bzmedia.widget.BZNativeSurfaceView;

import java.io.IOException;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class MediaRecordActivity extends AppCompatActivity {
    private Camera mCamera;
    private int EXPECT_RECORD_WIDTH = 720;
    private int cameraId = Camera.CameraInfo.CAMERA_FACING_FRONT;
    private BZNativeSurfaceView gl_surface_view;
    private SurfaceTexture mSurface;
    private static final String TAG = "bz_MRecordAct";
    Camera.Size preSizes = null;
    private VideoRecorderBase videoRecorderMediaCode;
    private Viewport viewport;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_meidia_record);
        gl_surface_view = (BZNativeSurfaceView) findViewById(R.id.bz_native_surface_view);
        gl_surface_view.addSurfaceCallback(new BZNativeSurfaceView.SurfaceCallback() {
            @Override
            public void surfaceCreated(SurfaceTexture mSurface) {
                MediaRecordActivity.this.mSurface = mSurface;
                startPriview();
                FilterInfo filterInfo = new FilterInfo();
                filterInfo.setFilterType(BZMedia.FilterType.WHITE_BALANCE);
                filterInfo.setFilterIntensity(50);
                gl_surface_view.switchFilter(filterInfo);
            }

            @Override
            public void onSurfaceChanged(int width, int height) {

            }
        });
        gl_surface_view.setOnDrawFrameListener(new BZNativeSurfaceView.OnDrawFrameListener() {
            @Override
            public void onDrawFrame(int lastTextureId) {
                if (null != videoRecorderMediaCode) {
                    videoRecorderMediaCode.updateTexture(lastTextureId);
                }
            }
        });
        gl_surface_view.setOnViewportCalcCompleteListener(new BZNativeSurfaceView.OnViewportCalcCompleteListener() {
            @Override
            public void onViewportCalcCompleteListener(Viewport viewport) {
                MediaRecordActivity.this.viewport = viewport;
            }
        });
    }

    //保证从大到小排列
    private Comparator<Camera.Size> comparatorBigger = new Comparator<Camera.Size>() {
        @Override
        public int compare(Camera.Size lhs, Camera.Size rhs) {
            int w = rhs.width - lhs.width;
            if (w == 0)
                return rhs.height - lhs.height;
            return w;
        }
    };

    private void startPriview() {
        if (cameraId == Camera.CameraInfo.CAMERA_FACING_FRONT) {
            cameraId = Camera.CameraInfo.CAMERA_FACING_BACK;
        } else if (cameraId == Camera.CameraInfo.CAMERA_FACING_BACK) {
            cameraId = Camera.CameraInfo.CAMERA_FACING_FRONT;
        }
        if (null != mCamera) {
            mCamera.release();
        }
        mCamera = Camera.open(cameraId);
        Camera.Parameters params = mCamera.getParameters();

        List<Integer> supportedPreviewFormats = params.getSupportedPreviewFormats();

        List<Camera.Size> supportedPreviewSizes = params.getSupportedPreviewSizes();
        Collections.sort(supportedPreviewSizes, comparatorBigger);
        for (Camera.Size previewSize : supportedPreviewSizes) {
            if (preSizes == null || (previewSize.width >= EXPECT_RECORD_WIDTH && previewSize.height >= EXPECT_RECORD_WIDTH)) {
                preSizes = previewSize;
            }
            BZLogUtil.d(TAG, "supportedPreviewSizes -width=" + previewSize.width + "--=" + previewSize.height);
        }

        List<Integer> supportedPreviewFrameRates = params.getSupportedPreviewFrameRates();
        for (Integer frameRate : supportedPreviewFrameRates) {
            Log.d(TAG, "frameRate=" + frameRate);
        }
//        preSizes.width = 720;
//        preSizes.height = 480;

        params.setPreviewSize(preSizes.width, preSizes.height);
        //由于是旋转了90度,所以倒置
        BZLogUtil.d(TAG, "setPreviewSize--width=" + preSizes.width + "--height=" + preSizes.height);
        mCamera.setParameters(params);


        gl_surface_view.setSrcRotation(90);
        gl_surface_view.setVideoSize(preSizes.width, preSizes.height);
        gl_surface_view.setFlip(false, Camera.CameraInfo.CAMERA_FACING_FRONT == cameraId);


        //采用Surface 这个参数就没用了
//       mCamera.setDisplayOrientation(90);
        try {
            mCamera.setPreviewTexture(mSurface);
        } catch (IOException e) {
            e.printStackTrace();
        }
        mCamera.startPreview();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (null != mCamera) {
            mCamera.stopPreview();
            mCamera.release();
        }
        gl_surface_view.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        gl_surface_view.onResume();
    }

    public void stopRecord(View view) {
        BZLogUtil.d(TAG, "stopRecord");
        if (null != videoRecorderMediaCode) {
            videoRecorderMediaCode.stopRecord();
            videoRecorderMediaCode = null;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (null != videoRecorderMediaCode) {
            videoRecorderMediaCode.stopRecord();
            videoRecorderMediaCode = null;
        }
    }

    public void startRecord(View view) {
        BZLogUtil.d(TAG, "startRecord");
        if (null != videoRecorderMediaCode) {
            stopRecord(null);
        }
//        VideoSize videoSize = VideoTacticsManager.getFitVideoSize(preSizes.width, preSizes.height);

        VideoSize videoSize = new VideoSize(preSizes.width, preSizes.height);
//        VideoSize videoSize = new VideoSize(720, 1280);

        final String path = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Filter/temp_" + System.currentTimeMillis() + ".mp4";
        BZFileUtils.createNewFile(path);
        videoRecorderMediaCode = VideoRecorderBuild.build();
//        videoRecorderMediaCode=new VideoRecorderNative();
//        videoRecorderMediaCode=new VideoRecorderMediaCodec();
        videoRecorderMediaCode.setGLSuerfaceView(gl_surface_view);
//        videoRecorderMediaCode.setAllFrameIsKey(true);

        videoRecorderMediaCode.setOnRecorderErrorListener(new OnRecorderErrorListener() {
            @Override
            public void onVideoError(int what, int extra) {
                BZLogUtil.e(TAG, "what=" + what + "--extra=" + extra);
            }

            @Override
            public void onAudioError(int what, String message) {
                BZLogUtil.e(TAG, "what=" + what + "--extra=" + message);
            }
        });
        videoRecorderMediaCode.setOnVideoRecorderStateListener(new VideoRecorderBase.OnVideoRecorderStateListener() {
            @Override
            public void onVideoRecorderStarted(boolean b) {

            }

            @Override
            public void onVideoRecorderStopped(boolean b) {
                Log.e(TAG, "onVideoRecorderStopped() called with: b = [" + b + "]");
                Intent intent = new Intent(MediaRecordActivity.this, MainActivity.class);
                intent.putExtra("filepath", path);
                startActivity(intent);
                MediaRecordActivity.this.finish();
            }
        });
        videoRecorderMediaCode.setPreviewWidth(videoSize.width);
        videoRecorderMediaCode.setPreviewHeight(videoSize.height);
        videoRecorderMediaCode.setRecordWidth(videoSize.width);
        videoRecorderMediaCode.setRecordHeight(videoSize.height);
        videoRecorderMediaCode.setVideoRotate(90);
        videoRecorderMediaCode.setVideoRate(30);
        videoRecorderMediaCode.setNeedFlipVertical(true);
        videoRecorderMediaCode.setRecordPixelFormat(BZMedia.PixelFormat.TEXTURE);
        videoRecorderMediaCode.setNeedAudio(true);
        videoRecorderMediaCode.setAllFrameIsKey(false);
        videoRecorderMediaCode.startRecord(path);
    }
}
