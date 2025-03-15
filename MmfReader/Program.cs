using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Globalization;

namespace MmfReader
{
    static class Program
    {
        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new Form1());
        }

        public static bool ReadFile(int index, ref MmfData data)
        {
            string fileName;
            int size;
            switch (index)
            {
                case 1:
                    fileName = "Local\\SimRacingStudioMotionRigPose";
                    size = Marshal.SizeOf<MmfData>();
                    break;
                case 2:
                    fileName = "Local\\motionRigPose";
                    size = Marshal.SizeOf<MmfData>();
                    break;
                case 3:
                    fileName = "Local\\YawVRGEFile";
                    size = Marshal.SizeOf<YawData>();
                    break;
                case 4:
	                fileName = "Local\\OXRMC_Telemetry";
	                size = Marshal.SizeOf<MmfData>();
	                break;
				default:
                    return false;
            }
            try
            {
                using MemoryMappedFile file = MemoryMappedFile.OpenExisting(fileName);
                using MemoryMappedViewAccessor accessor = file.CreateViewAccessor(0, size);
                bool success = false;
                switch (index)
                {
                    case 1:
                    case 2:
                        accessor.Read(0, out data);
                        success = true;
                        break;
                    case 3:
                        accessor.Read(0, out YawData yaw);
                        data.sway = 0;
                        data.surge = 0;
                        data.heave = 0;
                        data.yaw = yaw.yaw;
                        data.roll = yaw.roll;
                        data.pitch = yaw.pitch;
                        success = true;
                        break;
                    case 4:
	                    accessor.Read(0, out data);
	                    success = true;
	                    break;
					default:
                        break;
                }
                return success;
            }
            catch
            {
                return false;
            }
        }

        public static int curIndex = 0;
        public static CsvWriter writer = new();
    }

    class WorkerArguments
    {
        public int Interval { get; set; }
        public int Index { get; set; }
    }

    class WorkerResult
    {
        public bool Open { get; set; }
        public MmfData Data { get; set; }
    }

    public struct MmfData
    {
        public double sway, surge, heave, yaw, roll, pitch;
    }

    public struct YawData
    {
        public float yaw, pitch, roll, battery, rotationHeight, rotationForwardHead;
        public bool sixDof, usePos;
        public float autoX, autoY;
    };

    public class CsvWriter: IDisposable
    {
        StreamWriter writer = null;
        int counter = 0;

        public bool OpenFile(string fileName)
        {
            bool success = false;
            if ("" != fileName)
            {
                try
                {
                    string fullPath = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + @"\OpenXR-MotionCompensation\" + fileName + ".CSV";
                    writer = new StreamWriter(fullPath);
                    writer.WriteLine("count;timestamp;sway;surge;heave;yaw;roll;pitch");
                    writer.Flush();
                    counter = 0;
                    success = true;
                }
                catch (Exception)
                { }
            }
            return success;
        }
        public void WriteToFile(MmfData data)
        {
            if (null != writer)
            {
                DateTime localDate = DateTime.Now;
                var line = string.Format("{0};{1};{2};{3};{4};{5};{6};{7}", counter++, localDate.ToString("MM/dd/yyyy hh:mm:ss.fff tt"), data.sway, data.surge, data.heave, data.yaw, data.pitch, data.roll);
                writer.WriteLine(line);
                writer.Flush();
            }
        }
        public void CloseFile()
        {
            if (null != writer)
            {
                writer.Close();
                writer = null;
            }
        }

        public void Dispose()
        {
            if (null != writer)
            {
                writer.Close();
            }    
        }
    }
}
