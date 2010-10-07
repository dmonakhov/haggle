package org.haggle.LuckyMe;

import org.haggle.Interface;
import org.haggle.Node;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.AdapterView.AdapterContextMenuInfo;

public class LuckyView extends Activity
{
	private NodeAdapter nodeAdpt = null;
	private LuckyService mLuckyService = null;
	private boolean mIsBound = false;
	
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);   
        
        ListView neighlist = (ListView) findViewById(R.id.neighbor_list);
        
        nodeAdpt = new NodeAdapter(this);
        neighlist.setAdapter(nodeAdpt);

        // We also want to show context menu for longpressed items in the neighbor list
        registerForContextMenu(neighlist);
        startService(new Intent(this, LuckyService.class));
    }
    @Override
	public void onCreateContextMenu(ContextMenu menu, View v,
			ContextMenuInfo menuInfo) {
		// TODO We should check for the correct view like for the gallery
		/*
		 * ListView lv = (ListView) v; NodeAdapter na = (NodeAdapter)
		 * lv.getAdapter();
		 */
		menu.setHeaderTitle("Node Information");
		menu.add("Interfaces");
		menu.add("Cancel");
	}

	@Override
	public boolean onContextItemSelected(MenuItem item) {
		AdapterContextMenuInfo info = (AdapterContextMenuInfo) item
				.getMenuInfo();
		if (item.getTitle() == "Interfaces") {
			AlertDialog.Builder builder;
			AlertDialog alertDialog;

			Context mContext = getApplicationContext();
			LayoutInflater inflater = (LayoutInflater) mContext
					.getSystemService(LAYOUT_INFLATER_SERVICE);
			View layout = inflater.inflate(R.layout.neighbor_list_context_dialog,
					(ViewGroup) findViewById(R.id.layout_root));

			TextView text = (TextView) layout.findViewById(R.id.text);
			String t = "";
			Node node = nodeAdpt.getNode(info.position);

			if (node != null) {
				Interface[] ifaces = node.getInterfaces();

				for (int i = 0; i < ifaces.length; i++) {
					t += ifaces[i].getTypeString() + " "
							+ ifaces[i].getIdentifierString() + " "
							+ ifaces[i].getStatusString() + "\n";
				}
			}

			text.setText(t);
			builder = new AlertDialog.Builder(this);
			builder.setView(layout);
			alertDialog = builder.create();
			alertDialog.setTitle("Node Interfaces");
			alertDialog.show();
		}
		return true;
	}
	private ServiceConnection mConnection = new ServiceConnection() {
	    public void onServiceConnected(ComponentName className, IBinder service) {
	        // This is called when the connection with the service has been
	        // established, giving us the service object we can use to
	        // interact with the service.  Because we have bound to a explicit
	        // service that we know is running in our own process, we can
	        // cast its IBinder to a concrete class and directly access it.
	        mLuckyService = ((LuckyService.LuckyBinder)service).getService();

			Log.i("LuckyView", "Connected to LuckyService.");
	        // Tell the user about this for our demo.
	        //Toast.makeText(Binding.this, R.string.local_service_connected,
	              //  Toast.LENGTH_SHORT).show();
	    }

	    public void onServiceDisconnected(ComponentName className) {
	        // This is called when the connection with the service has been
	        // unexpectedly disconnected -- that is, its process crashed.
	        // Because it is running in our same process, we should never
	        // see this happen.
	        mLuckyService = null;
	       // Toast.makeText(Binding.this, R.string.local_service_disconnected,
	              //  Toast.LENGTH_SHORT).show();
	    	Log.i("LuckyView", "Disconnecte from LuckyService.");
	    }
	};

	void doBindService() {
	    // Establish a connection with the service.  We use an explicit
	    // class name because we want a specific service implementation that
	    // we know will be running in our own process (and thus won't be
	    // supporting component replacement by other applications).
	    bindService(new Intent(this, LuckyService.class), mConnection, Context.BIND_AUTO_CREATE);
	    mIsBound = true;
	}

	void doUnbindService() {
	    if (mIsBound) {
	        // Detach our existing connection.
	        unbindService(mConnection);
	        mIsBound = false;
	    }
	}

	@Override
	protected void onDestroy() {
		// TODO Auto-generated method stub
		super.onDestroy();
		doUnbindService();
	}

	@Override
	protected void onStart() {
		// TODO Auto-generated method stub
		super.onStart();
		doBindService();
	}

	@Override
	protected void onStop() {
		// TODO Auto-generated method stub
		super.onStop();
	}
    
}
