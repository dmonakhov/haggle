namespace LuckyGUI
{
	partial class TestControlWindow
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;
		private System.Windows.Forms.MainMenu mainMenu1;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
                        this.mainMenu1 = new System.Windows.Forms.MainMenu();
                        this.menuBack = new System.Windows.Forms.MenuItem();
                        this.menuItem1 = new System.Windows.Forms.MenuItem();
                        this.menuItemKillHaggle = new System.Windows.Forms.MenuItem();
                        this.menuItemQuitLuckyMe = new System.Windows.Forms.MenuItem();
                        this.testStageLabel = new System.Windows.Forms.Label();
                        this.start_button = new System.Windows.Forms.Button();
                        this.stop_button = new System.Windows.Forms.Button();
                        this.label2 = new System.Windows.Forms.Label();
                        this.label3 = new System.Windows.Forms.Label();
                        this.shutdown_button = new System.Windows.Forms.Button();
                        this.label4 = new System.Windows.Forms.Label();
                        this.statusMsgLabel = new System.Windows.Forms.Label();
                        this.SuspendLayout();
                        // 
                        // mainMenu1
                        // 
                        this.mainMenu1.MenuItems.Add(this.menuBack);
                        this.mainMenu1.MenuItems.Add(this.menuItem1);
                        // 
                        // menuBack
                        // 
                        this.menuBack.Text = "Back";
                        this.menuBack.Click += new System.EventHandler(this.menuBack_Click);
                        // 
                        // menuItem1
                        // 
                        this.menuItem1.MenuItems.Add(this.menuItemKillHaggle);
                        this.menuItem1.MenuItems.Add(this.menuItemQuitLuckyMe);
                        this.menuItem1.Text = "Force";
                        // 
                        // menuItemKillHaggle
                        // 
                        this.menuItemKillHaggle.Text = "Kill Haggle";
                        this.menuItemKillHaggle.Click += new System.EventHandler(this.kill_haggle_Click);
                        // 
                        // menuItemQuitLuckyMe
                        // 
                        this.menuItemQuitLuckyMe.Text = "Quit LuckyMe";
                        this.menuItemQuitLuckyMe.Click += new System.EventHandler(this.quit_luckyme_Click);
                        // 
                        // testStageLabel
                        // 
                        this.testStageLabel.Location = new System.Drawing.Point(4, 4);
                        this.testStageLabel.Name = "testStageLabel";
                        this.testStageLabel.Size = new System.Drawing.Size(233, 20);
                        this.testStageLabel.Text = "Test stage: NOT_RUNNING";
                        // 
                        // start_button
                        // 
                        this.start_button.Location = new System.Drawing.Point(41, 72);
                        this.start_button.Name = "start_button";
                        this.start_button.Size = new System.Drawing.Size(159, 34);
                        this.start_button.TabIndex = 1;
                        this.start_button.Text = "Start test";
                        this.start_button.Click += new System.EventHandler(this.button_start_Click);
                        // 
                        // stop_button
                        // 
                        this.stop_button.Enabled = false;
                        this.stop_button.Location = new System.Drawing.Point(41, 144);
                        this.stop_button.Name = "stop_button";
                        this.stop_button.Size = new System.Drawing.Size(159, 34);
                        this.stop_button.TabIndex = 2;
                        this.stop_button.Text = "Stop test";
                        this.stop_button.Click += new System.EventHandler(this.button_stop_Click);
                        // 
                        // label2
                        // 
                        this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
                        this.label2.Location = new System.Drawing.Point(4, 52);
                        this.label2.Name = "label2";
                        this.label2.Size = new System.Drawing.Size(196, 18);
                        this.label2.Text = "Clicking \'Start test\' will start a new test";
                        // 
                        // label3
                        // 
                        this.label3.Location = new System.Drawing.Point(4, 110);
                        this.label3.Name = "label3";
                        this.label3.Size = new System.Drawing.Size(218, 32);
                        this.label3.Text = "Clicking \'Stop test\' will abort the current test and store all logs.";
                        // 
                        // shutdown_button
                        // 
                        this.shutdown_button.Location = new System.Drawing.Point(41, 220);
                        this.shutdown_button.Name = "shutdown_button";
                        this.shutdown_button.Size = new System.Drawing.Size(159, 34);
                        this.shutdown_button.TabIndex = 5;
                        this.shutdown_button.Text = "Shutdown";
                        this.shutdown_button.Click += new System.EventHandler(this.button_shutdown_Click);
                        // 
                        // label4
                        // 
                        this.label4.Location = new System.Drawing.Point(3, 185);
                        this.label4.Name = "label4";
                        this.label4.Size = new System.Drawing.Size(233, 34);
                        this.label4.Text = "Clicking \'Shutdown\' will terminate both Haggle and this program.";
                        this.label4.ParentChanged += new System.EventHandler(this.label4_ParentChanged);
                        // 
                        // statusMsgLabel
                        // 
                        this.statusMsgLabel.Location = new System.Drawing.Point(4, 24);
                        this.statusMsgLabel.Name = "statusMsgLabel";
                        this.statusMsgLabel.Size = new System.Drawing.Size(232, 20);
                        this.statusMsgLabel.Text = "Error:";
                        this.statusMsgLabel.ParentChanged += new System.EventHandler(this.label1_ParentChanged);
                        // 
                        // TestControlWindow
                        // 
                        this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
                        this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
                        this.AutoScroll = true;
                        this.ClientSize = new System.Drawing.Size(240, 268);
                        this.Controls.Add(this.statusMsgLabel);
                        this.Controls.Add(this.label4);
                        this.Controls.Add(this.shutdown_button);
                        this.Controls.Add(this.label3);
                        this.Controls.Add(this.label2);
                        this.Controls.Add(this.stop_button);
                        this.Controls.Add(this.start_button);
                        this.Controls.Add(this.testStageLabel);
                        this.Menu = this.mainMenu1;
                        this.Name = "TestControlWindow";
                        this.Text = "Test Controls";
                        this.Load += new System.EventHandler(this.TestControlWindow_Load);
                        this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.MenuItem menuBack;
		private System.Windows.Forms.Button stop_button;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.Button shutdown_button;
		private System.Windows.Forms.Label label4;
		public System.Windows.Forms.Label testStageLabel;
		public System.Windows.Forms.Button start_button;
		public System.Windows.Forms.Label label2;
                private System.Windows.Forms.Label statusMsgLabel;
                private System.Windows.Forms.MenuItem menuItem1;
                private System.Windows.Forms.MenuItem menuItemKillHaggle;
                private System.Windows.Forms.MenuItem menuItemQuitLuckyMe;
	}
}