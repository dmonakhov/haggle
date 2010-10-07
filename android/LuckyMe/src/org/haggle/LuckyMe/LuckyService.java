package org.haggle.LuckyMe;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

import org.haggle.Attribute;
import org.haggle.DataObject;
import org.haggle.EventHandler;
import org.haggle.Handle;
import org.haggle.LaunchCallback;
import org.haggle.Node;
import org.haggle.DataObject.DataObjectException;
import org.haggle.Handle.AlreadyRegisteredException;
import org.haggle.Handle.RegistrationFailedException;

public class LuckyService extends Service implements EventHandler {
	private NotificationManager mNM;
	private Thread mDataThread = null;
	private DataObjectGenerator mDataGenerator = null;
	private org.haggle.Handle hh = null;
	private boolean haggleIsRunning = false;
	private long num_dataobjects_rx = 0;
	private long num_dataobjects_tx = 0;

	@Override
	public void onCreate() {
		super.onCreate();
	}
	@Override
	public void onDestroy() {
		super.onDestroy();
		stopDataGenerator();
		if (hh != null) {
			hh.dispose();
			hh = null;
		}
		Log.i("LuckyService", "Service destroyed.");
	}
	
	@Override
	public boolean onUnbind(Intent intent) {
		Log.i("LuckyService", "LuckyMe disconnected from service.");
		return super.onUnbind(intent);
	}
	
	private void stopDataGenerator()
	{
		if (mDataThread != null) {
			mDataGenerator.stop();
			try {
				mDataThread.join();
				Log.i("LuckyService", "Joined with data thread.");
			} catch (InterruptedException e) {
				Log.i("LuckyService", "Could not join with data thread.");
			}
		}
	}
	/**
     * Class for clients to access.  Because we know this service always
     * runs in the same process as its clients, we don't need to deal with
     * IPC.
     */
    public class LuckyBinder extends Binder {
        LuckyService getService() {
            return LuckyService.this;
        }
    }

	class DataObjectGenerator implements Runnable {
		private boolean shouldExit = false;
		private long luck = 1;
		@Override
		public void run() {
			
			while (!shouldExit) {
				Log.d("LuckyService", "Generate data object");
				
				try {
					Thread.sleep(10000);
				} catch (InterruptedException e) {
					Log.d("LuckyService", "DataObjectGenerator interrupted");
					continue;
				}
				
				DataObject dObj;
				try {
					dObj = new DataObject();
					dObj.addAttribute("LuckyMe", "" + luck++);
					hh.publishDataObject(dObj);
				} catch (DataObjectException e) {
					
				}
			}
		}
		public void stop() {
			shouldExit = true;
		}
	}
	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		 Log.d("LuckyService", "Received start id " + startId + ": " + intent);
	     // We want this service to continue running until it is explicitly
	     // stopped, so return sticky.
		 
		 if (hh == null) {
			switch (Handle.getDaemonStatus()) {
			case Handle.HAGGLE_DAEMON_CRASHED:
				Log.d("LuckyService", "Haggle crashed, starting again");
			case Handle.HAGGLE_DAEMON_NOT_RUNNING:
				if (Handle.spawnDaemon(new LaunchCallback() {

					public int callback(long milliseconds) {

						Log.d("LuckyService", "Spawning milliseconds..." + milliseconds);

						if (milliseconds == 0) {
							// Daemon launched
						}
						return 0;
					}
				})) {
					Log.d("LuckyService", "Haggle daemon started");
					haggleIsRunning = true;
				} else {
					Log.d("LuckyService", "Haggle daemon start failed");
				}
				break;
			case Handle.HAGGLE_DAEMON_ERROR:
				Log.d("LuckyService", "Haggle daemon error");
				break;
			case Handle.HAGGLE_DAEMON_RUNNING:
				Log.d("LuckyService", "Haggle daemon already running");
				haggleIsRunning = true;
				break;
			}
			
			if (haggleIsRunning) {
				try {
					hh = new Handle("LuckyMe");
				} catch (AlreadyRegisteredException e) {
					Log.i("LuckyService", "Already registered.");
					e.printStackTrace();
					return START_STICKY;
				} catch (RegistrationFailedException e) {
					Log.i("LuckyService", "Registration failed.");
					e.printStackTrace();
					return START_STICKY;
				}
				
				hh.registerEventInterest(EVENT_HAGGLE_SHUTDOWN, this);
				hh.registerEventInterest(EVENT_INTEREST_LIST_UPDATE, this);
				hh.registerEventInterest(EVENT_NEW_DATAOBJECT, this);
				hh.registerEventInterest(EVENT_NEIGHBOR_UPDATE, this);
				
				hh.eventLoopRunAsync();
				mDataGenerator = new DataObjectGenerator();
				mDataThread = new Thread(mDataGenerator);
				mDataThread.start();
			}
		 }
	     return START_STICKY;
	}
	@Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    // This is the object that receives interactions from clients.
    private final IBinder mBinder = new LuckyBinder();
    
	/**
     * Show a notification while this service is running.
     */
    private void showNotification() {
        // We'll use the same text for the ticker and the expanded notification
        CharSequence text = getText(R.string.lucky_service_started);

        // Set the icon, scrolling text and timestamp
        //Notification notification = new Notification(R.drawable.stat_sample, text,
        //        System.currentTimeMillis());

        // The PendingIntent to launch our activity if the user selects this notification
        //PendingIntent contentIntent = PendingIntent.getActivity(this, 0,
         //       new Intent(this, LocalServiceActivities.Controller.class), 0);
       
        // Set the info for the views that show in the notification panel.
        //notification.setLatestEventInfo(this, getText(R.string.lucky_service_label),
        //               text, contentIntent);

        // Send the notification.
        // We use a layout id because it is a unique number.  We use it later to cancel.
        //mNM.notify(R.string.lucky_service_started, notification);
    }

	@Override
	public void onInterestListUpdate(Attribute[] interests) {
		Log.i("LuckyService", "Got interest list of " + interests.length + " items");
	}

	@Override
	public void onNeighborUpdate(Node[] neighbors) {
		Log.i("LuckyService", "New neighbor update.");
	}

	@Override
	public void onNewDataObject(DataObject dObj) {
		num_dataobjects_rx++;
		Log.i("LuckyService", "Data object received.");
	}

	@Override
	public void onShutdown(int reason) {
		Log.i("LuckyService", "Haggle was shut down.");
		stopSelf();
	}

	@Override
	public void onEventLoopStart() {
		Log.i("LuckyService", "Event loop started.");
		hh.getApplicationInterestsAsync();
	}

	@Override
	public void onEventLoopStop() {
		Log.i("LuckyService", "Event loop stopped.");
	}
}
