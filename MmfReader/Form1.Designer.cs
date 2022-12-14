
namespace MmfReader
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
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
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.comboBox1 = new System.Windows.Forms.ComboBox();
            this.backgroundWorker1 = new System.ComponentModel.BackgroundWorker();
            this.labelSelect = new System.Windows.Forms.Label();
            this.labelPitch = new System.Windows.Forms.Label();
            this.labelRoll = new System.Windows.Forms.Label();
            this.labelYaw = new System.Windows.Forms.Label();
            this.labelHeave = new System.Windows.Forms.Label();
            this.labelSway = new System.Windows.Forms.Label();
            this.labelSurge = new System.Windows.Forms.Label();
            this.labelHeaveVal = new System.Windows.Forms.Label();
            this.labelSwayVal = new System.Windows.Forms.Label();
            this.labelSurgeVal = new System.Windows.Forms.Label();
            this.labelYawVal = new System.Windows.Forms.Label();
            this.labelRollVal = new System.Windows.Forms.Label();
            this.labelPitchVal = new System.Windows.Forms.Label();
            this.labelHeaveUnit = new System.Windows.Forms.Label();
            this.labelSwayUnit = new System.Windows.Forms.Label();
            this.labelSurgeUnit = new System.Windows.Forms.Label();
            this.labelYawUnit = new System.Windows.Forms.Label();
            this.labelRollUnit = new System.Windows.Forms.Label();
            this.labelPitchUnit = new System.Windows.Forms.Label();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            this.tableLayoutPanel1.SuspendLayout();
            this.tableLayoutPanel2.SuspendLayout();
            this.SuspendLayout();
            // 
            // comboBox1
            // 
            this.comboBox1.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBox1.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.comboBox1.FormattingEnabled = true;
            this.comboBox1.Items.AddRange(new object[] {
            "-",
            "Sim Racing Studio",
            "FlyPT Mover",
            "Yaw VR / Yaw2"});
            this.comboBox1.Location = new System.Drawing.Point(365, 30);
            this.comboBox1.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.comboBox1.Name = "comboBox1";
            this.comboBox1.Size = new System.Drawing.Size(308, 46);
            this.comboBox1.TabIndex = 0;
            this.comboBox1.SelectedIndexChanged += new System.EventHandler(this.comboBox1_SelectedIndexChanged);
            // 
            // backgroundWorker1
            // 
            this.backgroundWorker1.DoWork += new System.ComponentModel.DoWorkEventHandler(this.backgroundWorker1_DoWork);
            this.backgroundWorker1.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.backgroundWorker1_RunWorkerCompleted);
            // 
            // labelSelect
            // 
            this.labelSelect.AutoSize = true;
            this.labelSelect.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelSelect.Location = new System.Drawing.Point(62, 32);
            this.labelSelect.Name = "labelSelect";
            this.labelSelect.Size = new System.Drawing.Size(274, 38);
            this.labelSelect.TabIndex = 1;
            this.labelSelect.Text = "Select virtual tracker:";
            // 
            // labelPitch
            // 
            this.labelPitch.AutoSize = true;
            this.labelPitch.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelPitch.Location = new System.Drawing.Point(66, 123);
            this.labelPitch.Name = "labelPitch";
            this.labelPitch.Size = new System.Drawing.Size(84, 38);
            this.labelPitch.TabIndex = 2;
            this.labelPitch.Text = "Pitch:";
            // 
            // labelRoll
            // 
            this.labelRoll.AutoSize = true;
            this.labelRoll.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelRoll.Location = new System.Drawing.Point(66, 220);
            this.labelRoll.Name = "labelRoll";
            this.labelRoll.Size = new System.Drawing.Size(69, 38);
            this.labelRoll.TabIndex = 3;
            this.labelRoll.Text = "Roll:";
            // 
            // labelYaw
            // 
            this.labelYaw.AutoSize = true;
            this.labelYaw.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelYaw.Location = new System.Drawing.Point(66, 326);
            this.labelYaw.Name = "labelYaw";
            this.labelYaw.Size = new System.Drawing.Size(70, 38);
            this.labelYaw.TabIndex = 4;
            this.labelYaw.Text = "Yaw:";
            // 
            // labelHeave
            // 
            this.labelHeave.AutoSize = true;
            this.labelHeave.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelHeave.Location = new System.Drawing.Point(432, 326);
            this.labelHeave.Name = "labelHeave";
            this.labelHeave.Size = new System.Drawing.Size(100, 38);
            this.labelHeave.TabIndex = 7;
            this.labelHeave.Text = "Heave:";
            // 
            // labelSway
            // 
            this.labelSway.AutoSize = true;
            this.labelSway.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelSway.Location = new System.Drawing.Point(432, 220);
            this.labelSway.Name = "labelSway";
            this.labelSway.Size = new System.Drawing.Size(86, 38);
            this.labelSway.TabIndex = 6;
            this.labelSway.Text = "Sway:";
            // 
            // labelSurge
            // 
            this.labelSurge.AutoSize = true;
            this.labelSurge.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelSurge.Location = new System.Drawing.Point(432, 123);
            this.labelSurge.Name = "labelSurge";
            this.labelSurge.Size = new System.Drawing.Size(95, 38);
            this.labelSurge.TabIndex = 5;
            this.labelSurge.Text = "Surge:";
            // 
            // labelHeaveVal
            // 
            this.labelHeaveVal.AutoSize = true;
            this.labelHeaveVal.Dock = System.Windows.Forms.DockStyle.Right;
            this.labelHeaveVal.Font = new System.Drawing.Font("Courier New", 12F);
            this.labelHeaveVal.ImageAlign = System.Drawing.ContentAlignment.TopLeft;
            this.labelHeaveVal.Location = new System.Drawing.Point(137, 194);
            this.labelHeaveVal.Name = "labelHeaveVal";
            this.labelHeaveVal.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.labelHeaveVal.Size = new System.Drawing.Size(31, 113);
            this.labelHeaveVal.TabIndex = 13;
            this.labelHeaveVal.Text = "X";
            this.labelHeaveVal.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // labelSwayVal
            // 
            this.labelSwayVal.AutoSize = true;
            this.labelSwayVal.Dock = System.Windows.Forms.DockStyle.Right;
            this.labelSwayVal.Font = new System.Drawing.Font("Courier New", 12F);
            this.labelSwayVal.ImageAlign = System.Drawing.ContentAlignment.TopLeft;
            this.labelSwayVal.Location = new System.Drawing.Point(137, 97);
            this.labelSwayVal.Name = "labelSwayVal";
            this.labelSwayVal.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.labelSwayVal.Size = new System.Drawing.Size(31, 97);
            this.labelSwayVal.TabIndex = 12;
            this.labelSwayVal.Text = "X";
            this.labelSwayVal.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // labelSurgeVal
            // 
            this.labelSurgeVal.AutoSize = true;
            this.labelSurgeVal.Dock = System.Windows.Forms.DockStyle.Right;
            this.labelSurgeVal.Font = new System.Drawing.Font("Courier New", 12F);
            this.labelSurgeVal.ImageAlign = System.Drawing.ContentAlignment.TopLeft;
            this.labelSurgeVal.Location = new System.Drawing.Point(137, 0);
            this.labelSurgeVal.Name = "labelSurgeVal";
            this.labelSurgeVal.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.labelSurgeVal.Size = new System.Drawing.Size(31, 97);
            this.labelSurgeVal.TabIndex = 11;
            this.labelSurgeVal.Text = "X";
            this.labelSurgeVal.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // labelYawVal
            // 
            this.labelYawVal.AutoSize = true;
            this.labelYawVal.Dock = System.Windows.Forms.DockStyle.Right;
            this.labelYawVal.Font = new System.Drawing.Font("Courier New", 12F);
            this.labelYawVal.ImageAlign = System.Drawing.ContentAlignment.TopLeft;
            this.labelYawVal.Location = new System.Drawing.Point(146, 194);
            this.labelYawVal.Name = "labelYawVal";
            this.labelYawVal.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.labelYawVal.Size = new System.Drawing.Size(31, 113);
            this.labelYawVal.TabIndex = 10;
            this.labelYawVal.Text = "X";
            this.labelYawVal.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // labelRollVal
            // 
            this.labelRollVal.AutoSize = true;
            this.labelRollVal.Dock = System.Windows.Forms.DockStyle.Right;
            this.labelRollVal.Font = new System.Drawing.Font("Courier New", 12F);
            this.labelRollVal.ImageAlign = System.Drawing.ContentAlignment.TopLeft;
            this.labelRollVal.Location = new System.Drawing.Point(146, 97);
            this.labelRollVal.Name = "labelRollVal";
            this.labelRollVal.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.labelRollVal.Size = new System.Drawing.Size(31, 97);
            this.labelRollVal.TabIndex = 9;
            this.labelRollVal.Text = "X";
            this.labelRollVal.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // labelPitchVal
            // 
            this.labelPitchVal.AutoSize = true;
            this.labelPitchVal.Dock = System.Windows.Forms.DockStyle.Right;
            this.labelPitchVal.Font = new System.Drawing.Font("Courier New", 12F);
            this.labelPitchVal.ImageAlign = System.Drawing.ContentAlignment.TopLeft;
            this.labelPitchVal.Location = new System.Drawing.Point(146, 0);
            this.labelPitchVal.Name = "labelPitchVal";
            this.labelPitchVal.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.labelPitchVal.Size = new System.Drawing.Size(31, 97);
            this.labelPitchVal.TabIndex = 8;
            this.labelPitchVal.Text = "X";
            this.labelPitchVal.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // labelHeaveUnit
            // 
            this.labelHeaveUnit.AutoSize = true;
            this.labelHeaveUnit.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelHeaveUnit.Location = new System.Drawing.Point(710, 326);
            this.labelHeaveUnit.Name = "labelHeaveUnit";
            this.labelHeaveUnit.Size = new System.Drawing.Size(41, 38);
            this.labelHeaveUnit.TabIndex = 19;
            this.labelHeaveUnit.Text = "m";
            this.labelHeaveUnit.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // labelSwayUnit
            // 
            this.labelSwayUnit.AutoSize = true;
            this.labelSwayUnit.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelSwayUnit.Location = new System.Drawing.Point(710, 220);
            this.labelSwayUnit.Name = "labelSwayUnit";
            this.labelSwayUnit.Size = new System.Drawing.Size(41, 38);
            this.labelSwayUnit.TabIndex = 18;
            this.labelSwayUnit.Text = "m";
            this.labelSwayUnit.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // labelSurgeUnit
            // 
            this.labelSurgeUnit.AutoSize = true;
            this.labelSurgeUnit.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelSurgeUnit.Location = new System.Drawing.Point(710, 123);
            this.labelSurgeUnit.Name = "labelSurgeUnit";
            this.labelSurgeUnit.Size = new System.Drawing.Size(41, 38);
            this.labelSurgeUnit.TabIndex = 17;
            this.labelSurgeUnit.Text = "m";
            this.labelSurgeUnit.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // labelYawUnit
            // 
            this.labelYawUnit.AutoSize = true;
            this.labelYawUnit.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelYawUnit.Location = new System.Drawing.Point(342, 326);
            this.labelYawUnit.Name = "labelYawUnit";
            this.labelYawUnit.Size = new System.Drawing.Size(28, 38);
            this.labelYawUnit.TabIndex = 16;
            this.labelYawUnit.Text = "°";
            this.labelYawUnit.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // labelRollUnit
            // 
            this.labelRollUnit.AutoSize = true;
            this.labelRollUnit.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelRollUnit.Location = new System.Drawing.Point(342, 220);
            this.labelRollUnit.Name = "labelRollUnit";
            this.labelRollUnit.Size = new System.Drawing.Size(28, 38);
            this.labelRollUnit.TabIndex = 15;
            this.labelRollUnit.Text = "°";
            this.labelRollUnit.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // labelPitchUnit
            // 
            this.labelPitchUnit.AutoSize = true;
            this.labelPitchUnit.Font = new System.Drawing.Font("Segoe UI", 12F);
            this.labelPitchUnit.Location = new System.Drawing.Point(342, 123);
            this.labelPitchUnit.Name = "labelPitchUnit";
            this.labelPitchUnit.Size = new System.Drawing.Size(28, 38);
            this.labelPitchUnit.TabIndex = 14;
            this.labelPitchUnit.Text = "°";
            this.labelPitchUnit.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel1.Controls.Add(this.labelPitchVal, 0, 0);
            this.tableLayoutPanel1.Controls.Add(this.labelRollVal, 0, 1);
            this.tableLayoutPanel1.Controls.Add(this.labelYawVal, 0, 2);
            this.tableLayoutPanel1.Location = new System.Drawing.Point(156, 98);
            this.tableLayoutPanel1.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 3;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 113F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(180, 307);
            this.tableLayoutPanel1.TabIndex = 20;
            // 
            // tableLayoutPanel2
            // 
            this.tableLayoutPanel2.ColumnCount = 1;
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.Controls.Add(this.labelSurgeVal, 0, 0);
            this.tableLayoutPanel2.Controls.Add(this.labelSwayVal, 0, 1);
            this.tableLayoutPanel2.Controls.Add(this.labelHeaveVal, 0, 2);
            this.tableLayoutPanel2.Location = new System.Drawing.Point(533, 98);
            this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.tableLayoutPanel2.Name = "tableLayoutPanel2";
            this.tableLayoutPanel2.RowCount = 3;
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 113F));
            this.tableLayoutPanel2.Size = new System.Drawing.Size(171, 307);
            this.tableLayoutPanel2.TabIndex = 20;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(11F, 24F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.SystemColors.Desktop;
            this.ClientSize = new System.Drawing.Size(803, 429);
            this.Controls.Add(this.tableLayoutPanel2);
            this.Controls.Add(this.tableLayoutPanel1);
            this.Controls.Add(this.labelHeaveUnit);
            this.Controls.Add(this.labelSwayUnit);
            this.Controls.Add(this.labelSurgeUnit);
            this.Controls.Add(this.labelYawUnit);
            this.Controls.Add(this.labelRollUnit);
            this.Controls.Add(this.labelPitchUnit);
            this.Controls.Add(this.labelHeave);
            this.Controls.Add(this.labelSway);
            this.Controls.Add(this.labelSurge);
            this.Controls.Add(this.labelYaw);
            this.Controls.Add(this.labelRoll);
            this.Controls.Add(this.labelPitch);
            this.Controls.Add(this.labelSelect);
            this.Controls.Add(this.comboBox1);
            this.ForeColor = System.Drawing.SystemColors.ButtonHighlight;
            this.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.Name = "Form1";
            this.Text = "OpenXR-MotionCompensation MMF Reader";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.tableLayoutPanel2.ResumeLayout(false);
            this.tableLayoutPanel2.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ComboBox comboBox1;
        private System.ComponentModel.BackgroundWorker backgroundWorker1;
        private System.Windows.Forms.Label labelSelect;
        private System.Windows.Forms.Label labelPitch;
        private System.Windows.Forms.Label labelRoll;
        private System.Windows.Forms.Label labelYaw;
        private System.Windows.Forms.Label labelHeave;
        private System.Windows.Forms.Label labelSway;
        private System.Windows.Forms.Label labelSurge;
        private System.Windows.Forms.Label labelHeaveVal;
        private System.Windows.Forms.Label labelSwayVal;
        private System.Windows.Forms.Label labelSurgeVal;
        private System.Windows.Forms.Label labelYawVal;
        private System.Windows.Forms.Label labelRollVal;
        private System.Windows.Forms.Label labelPitchVal;
        private System.Windows.Forms.Label labelHeaveUnit;
        private System.Windows.Forms.Label labelSwayUnit;
        private System.Windows.Forms.Label labelSurgeUnit;
        private System.Windows.Forms.Label labelYawUnit;
        private System.Windows.Forms.Label labelRollUnit;
        private System.Windows.Forms.Label labelPitchUnit;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
    }
}

