using System;
using System.ComponentModel;
using System.Threading;
using System.Windows.Forms;

namespace MmfReader
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

            WorkerArguments arguments = new WorkerArguments
            {
                Index = Program.curIndex,
                Interval = 1
            };
            backgroundWorker1.RunWorkerAsync(arguments);
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        private void backgroundWorker1_DoWork(object sender, DoWorkEventArgs e)
        {
            WorkerArguments argument = e.Argument as WorkerArguments;
            MmfData curData = new MmfData();
            bool open = Program.ReadFile(argument.Index, ref curData);
            Thread.Sleep(argument.Interval);
            WorkerResult result = new WorkerResult();
            result.Data = curData;
            result.Open = open;
            e.Result = result;
        }


        private void backgroundWorker1_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            WorkerResult result = e.Result as WorkerResult;
            if (!result.Open)
            {
                labelRollVal.Text = "X";
                labelPitchVal.Text = "X";
                labelYawVal.Text = "X";
                labelSurgeVal.Text = "X";
                labelSwayVal.Text = "X";
                labelHeaveVal.Text = "X";
            }
            else
            {
                labelRollVal.Text = result.Data.roll.ToString("F3");
                labelPitchVal.Text = result.Data.pitch.ToString("F3");
                labelYawVal.Text = result.Data.yaw.ToString("F3");
                labelSurgeVal.Text = result.Data.surge.ToString("F3");
                labelSwayVal.Text = result.Data.sway.ToString("F3");
                labelHeaveVal.Text = result.Data.heave.ToString("F3");
                Program.writer.WriteToFile(result.Data);
            }
            WorkerArguments arguments = new WorkerArguments
            {
                Index = Program.curIndex,
                Interval = 1
            };
            backgroundWorker1.RunWorkerAsync(arguments);
        }

        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            Program.curIndex = comboBox1.SelectedIndex;
        }

        private void checkBox1_CheckStateChanged(object sender, EventArgs e)
        {

        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            if (checkBox1.Checked)
            {
                if(!Program.writer.OpenFile(textBox1.Text))
                {
                    checkBox1.Checked = false;
                }
            }
            else
            {
                Program.writer.CloseFile();
            }
        }
    }
}
