package org.haggle.LuckyMe;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.HashSet;
import java.util.Random;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
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
	private Thread mDataThread = null;
	private DataObjectGenerator mDataGenerator = null;
	private org.haggle.Handle hh = null;
	private boolean haggleIsRunning = false;
	private int num_dataobjects_rx = 0;
	private int num_dataobjects_tx = 0;
	public final String LUCKY_SERVICE_TAG = "LuckyService";
	public final String LUCKYME_APPNAME = "LuckyMe";
	public boolean isRunning = false;
	private int attributePoolSize = 100;
	private int numDataObjectAttributes = 3;
	private int numInterestAttributes = 5;
	private Random prngData = new Random(); // TODO seed PRNG
	private Random prngInterests = new Random(); // TODO seed PRNG
	private boolean ignoreShutdown = false;
	private Messenger mClientMessenger = null;
	public static final int MSG_NEIGHBOR_UPDATE = 0;
	public static final int MSG_NUM_DATAOBJECTS_TX = 1;
	public static final int MSG_NUM_DATAOBJECTS_RX = 2;
	public static final int MSG_LUCKY_SERVICE_START = 3;
	public static final int MSG_LUCKY_SERVICE_STOP = 4;
	private Node[] neighbors = null;
	private boolean mClientIsBound = false;
	
	@Override
	public void onCreate() {
		super.onCreate();
		
		Log.i(LUCKY_SERVICE_TAG, "LuckyService created"); 
		mDataGenerator = new DataObjectGenerator();
		mDataThread = new Thread(mDataGenerator);
	}

	@Override
	public void onDestroy() {
		super.onDestroy();

		stopDataGenerator();
		
		if (hh != null) {
			// Tell Haggle to stop, but first make sure we ignore the shutdown
			// event since we dispose of the handle here
			ignoreShutdown = true;
			hh.shutdown();
			hh.dispose();
			hh = null;
		}
		
		Log.i(LUCKY_SERVICE_TAG, "Service destroyed.");
	}
	
	public void bindClient(Intent intent) {
		if (intent.hasExtra("messenger")) {
			mClientMessenger = intent.getParcelableExtra("messenger");
		}
		mClientIsBound = true;
	}
	@Override
    public IBinder onBind(Intent intent) {
		Log.i(LUCKY_SERVICE_TAG, "onBind: Client bound to service : " + intent);
		bindClient(intent);
        return mBinder;
    }

	@Override
	public void onRebind(Intent intent) {
		Log.i(LUCKY_SERVICE_TAG, "onRebind: Client bound to service : " + intent);
		super.onRebind(intent);
		bindClient(intent);
	}
	
	@Override
	public boolean onUnbind(Intent intent) {
		super.onUnbind(intent);
		Log.i(LUCKY_SERVICE_TAG, "onUnbind: LuckyMe disconnected from service.");
		mClientIsBound = false;
		mClientMessenger = null;
		return true;
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
    
	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		 Log.d(LUCKY_SERVICE_TAG, "Received start id " + startId + ": " + intent);
	     // We want this service to continue running until it is explicitly
	     // stopped, so return sticky.

		 if (hh == null) {
			switch (Handle.getDaemonStatus()) {
			case Handle.HAGGLE_DAEMON_CRASHED:
				Log.d(LUCKY_SERVICE_TAG, "Haggle crashed, starting again");
			case Handle.HAGGLE_DAEMON_NOT_RUNNING:
				if (!backupLogs()) {
					Log.d(LUCKY_SERVICE_TAG, "Could not backup log files");
				}
				if (Handle.spawnDaemon(new LaunchCallback() {

					public int callback(long milliseconds) {

						Log.d(LUCKY_SERVICE_TAG, "Spawning milliseconds..." + milliseconds);

						if (milliseconds == 0) {
							// Daemon launched
						}
						return 0;
					}
				})) {
					Log.d(LUCKY_SERVICE_TAG, "Haggle daemon started");
					haggleIsRunning = true;
				} else {
					Log.d(LUCKY_SERVICE_TAG, "Haggle daemon start failed");
				}
				break;
			case Handle.HAGGLE_DAEMON_ERROR:
				Log.d(LUCKY_SERVICE_TAG, "Haggle daemon error");
				break;
			case Handle.HAGGLE_DAEMON_RUNNING:
				Log.d(LUCKY_SERVICE_TAG, "Haggle daemon already running");
				haggleIsRunning = true;
				break;
			}
			
			if (haggleIsRunning) {
				int i = 0;
				
				while (true) {
					try {
						hh = new Handle("LuckyMe");
					} catch (AlreadyRegisteredException e) {
						Log.i(LUCKY_SERVICE_TAG, "Already registered.");
						Handle.unregister("LuckyMe");
						if (i++ > 0)
							return START_STICKY;
						continue;
					} catch (RegistrationFailedException e) {
						Log.i(LUCKY_SERVICE_TAG, "Registration failed.");
						e.printStackTrace();
						return START_STICKY;
					}
					break;
				}
				hh.registerEventInterest(EVENT_HAGGLE_SHUTDOWN, this);
				hh.registerEventInterest(EVENT_INTEREST_LIST_UPDATE, this);
				hh.registerEventInterest(EVENT_NEW_DATAOBJECT, this);
				hh.registerEventInterest(EVENT_NEIGHBOR_UPDATE, this);
				
				hh.eventLoopRunAsync(this);
				mDataThread.start();
				isRunning = true;
			}
		 }
	     return START_STICKY;
	}
	
	class DataObjectGenerator implements Runnable {
		private boolean shouldExit = false;
		
		@Override
		public void run() {
			try {
				Thread.sleep(10000);
			} catch (InterruptedException e) {
			}
			
			while (!shouldExit) {
				Log.d(LUCKY_SERVICE_TAG, "Generate data object");
				
				try {
					Thread.sleep(10000);
				} catch (InterruptedException e) {
					Log.d(LUCKY_SERVICE_TAG, "DataObjectGenerator interrupted");
					continue;
				}

				DataObject dObj = createRandomDataObject("/sdcard/luckyme.jpg");
				
				if (dObj != null) {
					hh.publishDataObject(dObj);
					num_dataobjects_tx++;
					sendClientMessage(MSG_NUM_DATAOBJECTS_TX);
				}
			}
		}
		public void stop() {
			shouldExit = true;
		}
	}
	
	private void stopDataGenerator()
	{
		if (mDataThread != null) {
			mDataGenerator.stop();
			mDataThread.interrupt();
			try {
				mDataThread.join();
				Log.i(LUCKY_SERVICE_TAG, "Joined with data thread.");
			} catch (InterruptedException e) {
				Log.i(LUCKY_SERVICE_TAG, "Could not join with data thread.");
			}
		}
	}
	
	public boolean isRunning() 
	{
		return isRunning;
	}
	
	private class FileBackupFilter implements FilenameFilter {
		private String suffix;
		
		FileBackupFilter(String suffix) 
		{
			this.suffix = suffix;
		}
		@Override
		public boolean accept(File dir, String filename) {
			if (filename.endsWith(suffix))
				return true;
			return false;
		}
	}
	
	private boolean backupFile(String filename)
	{
		final String backupDir = "/sdcard/LuckyMe";
		
		File dir = new File(backupDir);
		
		if (!dir.exists() && !dir.mkdirs()) {
			Log.d(LUCKY_SERVICE_TAG, "saveFile: Could not create " + backupDir);
			return false;
		}
		
		File f = new File(filename);
		
		if (!f.exists()) {
			Log.d(LUCKY_SERVICE_TAG, "saveFile: No file=" + filename);
			return false;
		}
		
		if (!f.isFile()) {
			Log.d(LUCKY_SERVICE_TAG, "saveFile: file=" + filename + " is not a file");
			return false;
		}
		
		File[] fileList = dir.listFiles(new FileBackupFilter(f.getName()));
		
		int backupNum = 0;
		
		for (int i = 0; i < fileList.length; i++) {
			int index = fileList[i].getName().indexOf('-');
			String num = fileList[i].getName().substring(0, index);
			Log.d(LUCKY_SERVICE_TAG, "Found backup num " + num + " of file " + f.getName());
			Integer n;
			
			try {
				n = Integer.decode(num);
			} catch (NumberFormatException e) {
				Log.d(LUCKY_SERVICE_TAG, "Could not parse integer: " + num);
				continue;
			}
				
			if (n.intValue() >= backupNum) {
				Log.d(LUCKY_SERVICE_TAG, "backupNum=" + n);
				backupNum = n + 1;
			}
		}

		String backupFileName = dir.getAbsolutePath() + "/" + Integer.toString(backupNum) + "-" + f.getName();
		
		File backupFile = new File(backupFileName);
		
		Log.d(LUCKY_SERVICE_TAG, "saveFile: file=" + backupFile.getAbsolutePath());
		
		// We must read the file and write it to the new location.
		// A simple move operation doesn't work across file systems,
		// so this solution is needed to move the file to the sdcard.
		FileInputStream fIn;
		
		try {
			fIn = new FileInputStream(f);
		} catch (FileNotFoundException e) {
			Log.d(LUCKY_SERVICE_TAG, "saveFile: Error opening input file=" + 
					f.getAbsolutePath() + " : " + e.getMessage());
			return false;
		}
		
		FileOutputStream fOut;
		
		try {
			fOut = new FileOutputStream(backupFile);
		} catch (FileNotFoundException e) {
			Log.d(LUCKY_SERVICE_TAG, "saveFile:Error opening output file=" + 
					backupFile.getAbsolutePath() + " : " + e.getMessage());
			return false;
		}

		byte buf[] = new byte[1024];
		long totBytes = 0;
		boolean ret = false;
		
		while (true) {
			try {
				int len = fIn.read(buf);
				
				if (len == -1) {
					// EOF
					fIn.close();
					fOut.close();
					Log.d(LUCKY_SERVICE_TAG, "saveFile: successfully wrote file=" + 
							backupFile.getAbsolutePath() + " totBytes=" + totBytes);
					ret = true;
					f.delete();
					break;
				}
				fOut.write(buf, 0, len);
				
				totBytes += len;
			} catch (IOException e) {
				Log.d(LUCKY_SERVICE_TAG, "Error writing file=" + 
						backupFile.getAbsolutePath() + " : " + e.getMessage());
				break;
			}
		}
		
		return ret;
	}
	
	private final String backupFiles[] = { 
				"/data/haggle/haggle.db", 
				"/data/haggle/trace.log"
				};
	
	private boolean backupLogs()
	{
		boolean ret = true;
		
		for (int i = 0; i < backupFiles.length; i++) {
			boolean tmp_ret = backupFile(backupFiles[i]);
			
			if (!tmp_ret) {
				Log.d(LUCKY_SERVICE_TAG, "Could not backup file: " + backupFiles[i]);
			}
			
			ret &= tmp_ret;
		}	
		return ret;
	}
	
	private synchronized void setNeighbors(Node[] neighbors)
	{
		this.neighbors = neighbors;
	}
	public synchronized Node[] getNeighbors()
	{
		if (neighbors != null) {
			return neighbors.clone();
		}
		return null;
	}
	private void sendClientMessage(int msgType)
	{
		if (!mClientIsBound || mClientMessenger == null)
			return;
		
		Message msg = Message.obtain(null, msgType);
      
        switch (msgType) {
         case LuckyService.MSG_NEIGHBOR_UPDATE:
         	break;
         case LuckyService.MSG_NUM_DATAOBJECTS_TX:
        	 msg.arg1 = num_dataobjects_tx;
         	break;
         case LuckyService.MSG_NUM_DATAOBJECTS_RX:
        	 msg.arg1 = num_dataobjects_rx;
         	break;
         case LuckyService.MSG_LUCKY_SERVICE_START:
         	break;
         case LuckyService.MSG_LUCKY_SERVICE_STOP:
         	break;
         default:
        	 return;
        }

        try {
			mClientMessenger.send(msg);
		} catch (RemoteException e) {
			Log.d(LUCKY_SERVICE_TAG, "Messange send failed");
		}
	}
	
	public void requestAllUpdateMessages()
	{
		sendClientMessage(MSG_NEIGHBOR_UPDATE);
		sendClientMessage(MSG_NUM_DATAOBJECTS_TX);
		sendClientMessage(MSG_NUM_DATAOBJECTS_RX);
	}
    // This is the object that receives interactions from clients.
    private final IBinder mBinder = new LuckyBinder();
    /*
    private DataObject createRandomDataObject()
    {
    	return createRandomDataObject(null);
    }
    */
    private DataObject createRandomDataObject(String filename)
    {
    	DataObject dObj = null;
    	
    	if (filename != null && filename.length() > 0) {
    		try {
				dObj = new DataObject(filename);
				dObj.addFileHash();
			} catch (DataObjectException e) {
				Log.d(LUCKY_SERVICE_TAG, "File '" + filename + "' does not exist");
				try {
					dObj = new DataObject();
				} catch (DataObjectException e1) {
					Log.d(LUCKY_SERVICE_TAG, "Could not create data object");
				}
			}
    	} else {
    		try {
				dObj = new DataObject();
			} catch (DataObjectException e1) {
				Log.d(LUCKY_SERVICE_TAG, "Could not create data object");
			}
    	}
    	
    	if (dObj == null)
    		return null;
    	
    	HashSet<Integer> values = new HashSet<Integer>();
    	
    	for (int i = 0; i < numDataObjectAttributes;) {
    		int value = prngData.nextInt(attributePoolSize);
    		
    		if (values.contains(value)) {
    			continue;
    		}
    		
    		values.add(value);
    		dObj.addAttribute(LUCKYME_APPNAME, "" + value);
    		
    		i++;
    	}
    	
    	dObj.setCreateTime();
    	
    	return dObj;
    }
    
    private double fac(long n)
    {
    	long i, t = 1;
        
        for (i = n; i > 1; i--)
                t *= i;
        
        return (double)t;
    }
    
	private Attribute[] createInterestsBinomial() {
		int luck = 0;
		long n, u;
		Attribute[] interests = new Attribute[numInterestAttributes];
		boolean useNodeNrLuck = false;
		double p;

		if (useNodeNrLuck) {
			luck = 0; // TODO: set node number
		} else {
			luck = prngInterests.nextInt(attributePoolSize);
		}
		p = 0.5;
		n = numInterestAttributes - 1;

		u = (long) (n * p);

		for (int i = 0; i < numInterestAttributes; i++) {
			long interest, weight;
			interest = (luck + i - u + attributePoolSize) % attributePoolSize;
			weight = (long) (100 * fac(n) / (fac(n - i) * fac(i))
					* Math.pow(p, i) * Math.pow(1 - p, n - i));
			interests[i] = new Attribute("LuckyMe", Long.toString(interest),
					weight);
		}
		return interests;
	}
	@Override
	public void onInterestListUpdate(Attribute[] interests) {
		Log.i(LUCKY_SERVICE_TAG, "Got interest list of " + interests.length + " items");
		
		if (interests.length > 0) {
			for (int i = 0; i < interests.length; i++) {
				Log.i(LUCKY_SERVICE_TAG, "Interest " + i + ":" + interests[i]);
			}
		} else {
			hh.registerInterests(createInterestsBinomial());
		}
	}

	@Override
	public void onNeighborUpdate(Node[] neighbors) {
		Log.i(LUCKY_SERVICE_TAG, "New neighbor update.");
		setNeighbors(neighbors);
		sendClientMessage(MSG_NEIGHBOR_UPDATE);
	}

	@Override
	public void onNewDataObject(DataObject dObj) {
		num_dataobjects_rx++;
		Log.i(LUCKY_SERVICE_TAG, "Data object received.");
		sendClientMessage(MSG_NUM_DATAOBJECTS_RX);
	}

	@Override
	public void onShutdown(int reason) {
		Log.i(LUCKY_SERVICE_TAG, "Haggle was shut down.");
		if (!ignoreShutdown)
			stopSelf();
	}

	@Override
	public void onEventLoopStart() {
		Log.i(LUCKY_SERVICE_TAG, "Event loop started.");
		hh.getApplicationInterestsAsync();
	}

	@Override
	public void onEventLoopStop() {
		Log.i(LUCKY_SERVICE_TAG, "Event loop stopped.");
	}
}
