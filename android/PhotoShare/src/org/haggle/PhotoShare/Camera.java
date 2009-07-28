package org.haggle.PhotoShare;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;

import android.app.Activity;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.Size;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.*;
import android.widget.ImageView;
import android.provider.MediaStore;

public class Camera extends Activity implements View.OnClickListener, SurfaceHolder.Callback {
	private android.hardware.Camera mCameraDevice;
	private CameraPreview mCameraPreview;
	private SurfaceHolder mSurfaceHolder = null;
	private ImageView mLastPictureButton, mShutterButton;
	private android.hardware.Camera.Parameters mParameters;
	private boolean mIsPreviewing = false;
	private int mSavedWidth, mSavedHeight;
	OrientationEventListener mOrientationListener;
    int mLastOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;
    private ShutterCallback mShutterCallback = new ShutterCallback();
    private RawPictureCallback mRawPictureCallback = new RawPictureCallback();
    private JpegPictureCallback mJpegPictureCallback = new JpegPictureCallback();
    private AutoFocusCallback mAutoFocusCallback = new AutoFocusCallback();
    private ContentResolver mContentResolver;
    private Thread writeFileThread = null;

    private static final int STATUS_IDLE = 0;
    private static final int STATUS_TAKING_PICTURE = 1;
    private int mStatus = STATUS_IDLE;    
    static final int SAVE_PHOTO_PROGRESS_DIALOG = 0;
    ProgressDialog progressDialog;

    public void onCreate(Bundle b) {
		super.onCreate(b);

		// Open camera in separate thread to reduce startup time 
		// (modeled on official Android camera app).
		Thread openCameraThread = new Thread(new Runnable() {
			public void run() {
				mCameraDevice = android.hardware.Camera.open();
			}
		});
		
		openCameraThread.start();

		this.mOrientationListener = new OrientationEventListener(this) {
			public void onOrientationChanged(int orientation) {
				if (orientation != ORIENTATION_UNKNOWN) {
					mLastOrientation = orientation;
				}
			}
		};

		Window win = getWindow();
        win.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.camera);

        mCameraPreview = (CameraPreview) findViewById(R.id.camera_preview);
       
        SurfaceHolder holder = mCameraPreview.getHolder();
        holder.addCallback(this);
        holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

        mLastPictureButton = (ImageView) findViewById(R.id.last_picture_button);
        mLastPictureButton.setOnClickListener(this);
        
        mShutterButton = (ImageView) findViewById(R.id.shutter_button);
        mShutterButton.setOnClickListener(this);

        mContentResolver = getContentResolver();

		// Join open camera thread to make sure it's finished
		try {
			openCameraThread.join();
		} catch (InterruptedException ex) {
		}
		android.hardware.Camera.Parameters parameters = mCameraDevice.getParameters();
	}
	public void onStart() {
		super.onStart();
		makeSureCameraIsOpen();
		
		
	}
	public void onStop() {
		super.onStop();
		closeCamera();
	}
	public void onDestroy() {
		super.onDestroy();
	}
	private void makeSureCameraIsOpen() {
		if (mCameraDevice == null) {
			mCameraDevice = android.hardware.Camera.open();
		}
	}
	private void closeCamera() {
		if (mCameraDevice != null) {
			mCameraDevice.release();
			mCameraDevice = null;
		}
	}

	public void startPreview(int w, int h, boolean start) {
		mCameraPreview.setVisibility(View.VISIBLE);
		// if we're creating the surface, start the preview as well.
		
		if (mSavedWidth == w && mSavedHeight == h && mIsPreviewing)
			return;
		
		if (isFinishing())
			return;
		
		// Save preview size here?
		
		if (!start)
			return;
		
		if (mIsPreviewing) {
			stopPreview();
		}
		
		try {
			mCameraDevice.setPreviewDisplay(mSurfaceHolder);
		} catch (IOException exception) {
			mCameraDevice.release();
			mCameraDevice = null;
			// TODO: add more exception handling logic here
			return;
		}

		// request the preview size, the hardware may not honor it,
		// if we depended on it we would have to query the size again
		mParameters = mCameraDevice.getParameters();
		mParameters.setPreviewSize(w, h);
		mSavedWidth = w;
		mSavedHeight = h;
		mParameters.setPictureFormat(PixelFormat.JPEG); 
		
		try {
			mCameraDevice.setParameters(mParameters);
		} catch (IllegalArgumentException e) {
			// Ignore this error, it happens in the simulator.
		}

		try {
			mCameraDevice.startPreview();
			mIsPreviewing = true;
		} catch (Throwable e) {
			// TODO handle exception
		}
	
	}
	
	public void stopPreview() {
		if (mCameraDevice == null || mIsPreviewing == false)
			return;
		
		mCameraDevice.stopPreview();
		mIsPreviewing = false;
	}
	
	public void restartPreview() {
		startPreview(mSavedWidth, mSavedHeight, true);
	}
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
		// This is called immediately after any structural changes 
		// (format or size) have been made to the surface.
		startPreview(width, height, holder.isCreating());
	}

	public void surfaceCreated(SurfaceHolder holder) {
		mSurfaceHolder = holder;
	}

	public void surfaceDestroyed(SurfaceHolder holder) {
		stopPreview();
		mSurfaceHolder = null;
		
	}
	public void onClick(View v) {
        switch (v.getId()) {
        case R.id.last_picture_button:
            Log.d(PhotoShare.LOG_TAG, "Clicked on last picture button");
            break;
        case R.id.shutter_button:
            Log.d(PhotoShare.LOG_TAG, "Clicked on shutter button");
            takePicture();
            break;

        }
	}
	private final class ShutterCallback implements android.hardware.Camera.ShutterCallback {
		public void onShutter() {

			mCameraPreview.setVisibility(View.INVISIBLE);
			// Resize the SurfaceView to the aspect-ratio of the still image
			// and so that we can see the full image that was taken.
			Size pictureSize = mParameters.getPictureSize();
			mCameraPreview.setAspectRatio(pictureSize.width, pictureSize.height);
		}
	};

	private final class RawPictureCallback implements PictureCallback {
		public void onPictureTaken(byte [] rawData, android.hardware.Camera camera) {

		}
	};
	
	protected Dialog onCreateDialog(int id) {
        switch(id) {
        case SAVE_PHOTO_PROGRESS_DIALOG:
            progressDialog = new ProgressDialog(this);
            progressDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
            progressDialog.setMessage("Saving picture...");
            return progressDialog;
        default:
            return null;
        }
    }

	final Handler handler = new Handler() {
        public void handleMessage(Message msg) {
            //progressDialog.setProgress(total);
            dismissDialog(SAVE_PHOTO_PROGRESS_DIALOG);
        }
    };

	private final class JpegPictureCallback implements PictureCallback {
		public void onPictureTaken(final byte [] jpegData, android.hardware.Camera camera) {
			File dir = new File("/sdcard/PhotoShare");
			final String filename = "photoshare-" + System.currentTimeMillis() + ".jpg";
			
			Log.d(PhotoShare.LOG_TAG, "Picture was taken");
			
			dir.mkdirs();
			
			// Try first on external storage, otherwise use application's private storage
			if (dir.canWrite() == false)
				dir = getFilesDir();
			
			final String filepath = dir + "/" + filename;
			
			Log.d(PhotoShare.LOG_TAG, "filepath is " + filepath);

			// Write file to disk in separate thread to speed up the time 
			// it takes to show the picture attribute view in case the user
			// quits the camera view directly after the picture is taken.
			// TODO The downside is that there is a small risk the file is not 
			// yet written if the user is really fast in publishing the file
			// to Haggle. We should probably add some means to signal that the
			// file is done, but let the user do other tasks meanwhile
			
			
			// Make sure previous write operation is no longer running
			if (writeFileThread != null && writeFileThread.isAlive()) {
				try {
					writeFileThread.join();
				} catch (InterruptedException e) {
					// TODO Auto-generated catch block
					return;
				}
			}
			
			final class WriteFileThread implements Runnable  {
				Handler mHandler;
				
				public WriteFileThread(Handler mHandler) {
					this.mHandler = mHandler;
				}
				public void run() {
					FileOutputStream fos;
					
					try {
						//fos = openFileOutput(filepath, Context.MODE_WORLD_READABLE);
						fos = new FileOutputStream(filepath);
					} catch (FileNotFoundException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
						return;
					} catch (Exception e) {
						Log.d(PhotoShare.LOG_TAG, "openFileOutput failed: " + e.getMessage());
						return;
					}
					
					try {
						fos.write(jpegData);
					} catch (IOException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
						return;
					}
					
					try {
						fos.close();
					} catch (IOException e1) {
						// TODO Auto-generated catch block
						e1.printStackTrace();
					}

					/*
					String strUri;
					try {
						ContentResolver cr = getContentResolver();
						strUri = MediaStore.Images.Media.insertImage(cr, filepath, filename, "testDescription");
					} catch (FileNotFoundException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
						Log.d(PhotoShare.LOG_TAG, "Could not insert image into media store");
						return;
					} 
					if (strUri == null) {
						Log.d(PhotoShare.LOG_TAG, "Could not store image");
						return;
					}
					Log.d(PhotoShare.LOG_TAG, "Inserted picture in MediaStore (" + strUri + ")");
					
					Uri uri = Uri.parse(strUri);

					Log.d(PhotoShare.LOG_TAG, "File path is " + uri.getPath());
					*/
					
	                Message msg = mHandler.obtainMessage();
	                mHandler.sendMessage(msg);
				}
			}
			
			Thread writeFileThread = new Thread(new WriteFileThread(handler));
			
            showDialog(SAVE_PHOTO_PROGRESS_DIALOG);

			writeFileThread.start();
			
			/*final BitmapFactory.Options picOptions = new BitmapFactory.Options();
			final Bitmap pic = BitmapFactory.decodeByteArray(jpegData, 0, jpegData.length, picOptions); 
			*/
			
			Intent i = new Intent();
			
			i.putExtra("filepath", filepath);
			setResult(RESULT_OK, i);
			
			mStatus = STATUS_IDLE;
		}
	};

	private final class AutoFocusCallback implements android.hardware.Camera.AutoFocusCallback {
		public void onAutoFocus(boolean focused, android.hardware.Camera camera) {

		}
	};

	public static int roundOrientation(int orientationInput) {
        int orientation = orientationInput;
        if (orientation == -1)
            orientation = 0;

        orientation = orientation % 360;
        int retVal;
        if (orientation < (0*90) + 45) {
            retVal = 0;
        } else if (orientation < (1*90) + 45) {
            retVal = 90;
        } else if (orientation < (2*90) + 45) {
            retVal = 180;
        } else if (orientation < (3*90) + 45) {
            retVal = 270;
        } else {
            retVal = 0;
        }

        return retVal;
    }
	
	public void takePicture() {
		 stopPreview();
		 mStatus = STATUS_TAKING_PICTURE;
		 final int latchedOrientation = roundOrientation(mLastOrientation + 90);

         // Quality 75 has visible artifacts, and quality 90 looks great but the files begin to
         // get large. 85 is a good compromise between the two.
         mParameters.set("jpeg-quality", 85);
         mParameters.set("rotation", latchedOrientation);
         mCameraDevice.setParameters(mParameters);
         
		 mCameraDevice.takePicture(mShutterCallback, mRawPictureCallback,  mJpegPictureCallback);
         restartPreview();
         // TODO we should probably finish the activity after the first picture...
         // but we should not do it too quickly. We should have time to show a preview
         // of the taken picture.
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		switch (keyCode) {
		case KeyEvent.KEYCODE_FOCUS:
			if (event.getRepeatCount() == 0) {
				// TODO implement autofocus
			}
			return true;
		case KeyEvent.KEYCODE_DPAD_CENTER:
			takePicture();
			return true;
		case KeyEvent.KEYCODE_BACK:
			if (mStatus == STATUS_TAKING_PICTURE)
				return true;
			
			if (writeFileThread != null && writeFileThread.isAlive()) {
				// TODO find out why progress dialog is not shown

				Log.d(PhotoShare.LOG_TAG, "joining writeFileThread");
				
				try {
					writeFileThread.join();
				} catch (InterruptedException e) {
					return false;
				}
				Log.d(PhotoShare.LOG_TAG, "writeFileThread joined");
			}
			
			break;
		}
		return super.onKeyDown(keyCode, event);
	} 
}
