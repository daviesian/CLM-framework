﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

using ManagedMindReaderLib;
using System.Threading;
using System.Diagnostics;

namespace CLMAV
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {

        #region High-Resolution Timing
        static DateTime startTime;
        static Stopwatch sw = new Stopwatch();
        static MainWindow()
        {
            startTime = DateTime.Now;
            sw.Start();
        }
        public static DateTime CurrentTime
        {
            get { return startTime + sw.Elapsed; }
        }
        #endregion

        public static string[] args;
        MMR mmr;
        bool closing = false;
        bool resetTracking = false;
        FpsTracker fps = new FpsTracker();

        public MainWindow()
        {
            InitializeComponent();

            new Thread(delegate()
            {

                int camera = 0;
                for (int i = 0; i < args.Length; i++)
                {
                    if ((args[i] == "-c" || args[i] == "--camera") && args.Length > i + 1)
                        camera = int.Parse(args[i + 1]);
                }

                int mjpegServerPort = 0;
                for (int i = 0; i < args.Length - 1; i++)
                {
                    if ((args[i] == "-m" || args[i] == "--mjpeg") && args.Length > i + 1)
                    {
                        mjpegServerPort = Int32.Parse(args[i + 1]);
                        break;
                    }
                }

                bool dim_visible = false;

                for (int i = 0; i < args.Length; i++)
                {
                    if ((args[i] == "-width" && args.Length > i + 1))
                    {
                        int width = Int32.Parse(args[i + 1]);
                        Application.Current.Dispatcher.BeginInvoke(new ThreadStart(() => Application.Current.MainWindow.Width = width), null);
                        i++;
                    }
                    else if ((args[i] == "-height" && args.Length > i + 1))
                    {
                        int height = Int32.Parse(args[i + 1]);
                        Application.Current.Dispatcher.BeginInvoke(new ThreadStart(() => Application.Current.MainWindow.Height = height), null);
                        i++;
                    }
                    else if ((args[i] == "-xpos" && args.Length > i + 1))
                    {
                        int xpos = Int32.Parse(args[i + 1]);
                        Application.Current.Dispatcher.BeginInvoke(new ThreadStart(() => Application.Current.MainWindow.Left = xpos), null);
                        i++;
                    }
                    else if ((args[i] == "-ypos" && args.Length > i + 1))
                    {
                        int ypos = Int32.Parse(args[i + 1]);
                        Application.Current.Dispatcher.BeginInvoke(new ThreadStart(() => Application.Current.MainWindow.Top = ypos), null);
                        i++;
                    }
                    else if ((args[i] == "-showav"))
                    {
                        dim_visible = true;
                    }
                }

                // Somewhat hacky but works
                if (!dim_visible)
                {
                    Application.Current.Dispatcher.BeginInvoke(new ThreadStart(() => avPlot.Visibility = System.Windows.Visibility.Collapsed), null);
                    Application.Current.Dispatcher.BeginInvoke(new ThreadStart(() => analysisBox.Visibility = System.Windows.Visibility.Collapsed), null);
                    Application.Current.Dispatcher.BeginInvoke(new ThreadStart(() => OverallGrid.ColumnDefinitions.RemoveAt(3)), null);
                    Application.Current.Dispatcher.BeginInvoke(new ThreadStart(() => OverallGrid.ColumnDefinitions.RemoveAt(3)), null);                    
                }

                mmr = new MMR(mjpegServerPort);

                // This should be specified by the call? TODO using -camera param
                mmr.Init(camera);

                WriteableBitmap img = null;

                while (!closing)
                {
                    var f = mmr.GetNextFrame();
                    fps.AddFrame();

                    var success = mmr.TrackCLM();

                    if (success)
                    {

                        mmr.DrawCLMResult();
                        mmr.DoSmoothing();

                        var result = mmr.PredictDimensions();
                        
                        if (result != null)
                        {
                            Console.WriteLine(result.a + "\t" + result.e + "\t" + result.p + "\t" + result.v);
                            var dp = new DataPoint();
                            dp.values["a"] = result.a;
                            dp.values["e"] = result.e;
                            dp.values["p"] = result.p;
                            dp.values["v"] = result.v;
                            avPlot.AddDataPoint(dp);
                        }
                    }
                    
                    try
                    {
                        Dispatcher.Invoke(delegate()
                        {

                            if (dim_visible)
                            {
                                if (!mmr.PredictionAvailable)
                                {
                                    avPlot.Visibility = System.Windows.Visibility.Hidden;
                                    analysisBox.Visibility = System.Windows.Visibility.Visible;
                                }
                                else
                                {
                                    avPlot.Visibility = System.Windows.Visibility.Visible;
                                    analysisBox.Visibility = System.Windows.Visibility.Hidden;
                                }
                            }

                            if (img == null)
                            {
                                img = f.CreateWriteableBitmap();
                            }

                            f.UpdateWriteableBitmap(img);

                            imgTracking.Source = img;

                            Title = "ASC-Inclusion Facial Expression Analysis - " + fps.GetFPS().ToString("0.0") + " FPS";
                        });
                    }
                    catch { }
                    
                }
                mmr.Quit();
            }).Start();

        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            closing = true;
        }

    }
}
