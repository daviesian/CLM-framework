using System;
using System.Collections.Generic;
using System.Diagnostics;
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
using System.Windows.Threading;

namespace CLMAV
{
    public class DataPoint
    {
        public DataPoint()
        {
            Time = AVPlot.CurrentTime;

        }
        public DateTime Time { get; set; }

        public Dictionary<string, double> values = new Dictionary<string, double>();
    }
    /// <summary>
    /// Interaction logic for TimeSeriesPlot.xaml
    /// </summary>
    public partial class AVPlot : UserControl
    {

        #region High-Resolution Timing
        static DateTime startTime;
        static Stopwatch sw = new Stopwatch();
        static AVPlot()
        {
            startTime = DateTime.Now;
            sw.Start();
        }
        public static DateTime CurrentTime
        {
            get { return startTime + sw.Elapsed; }
        }
        #endregion

        double MaxAbsV = 1;
        double MaxAbsA = 1;

        Queue<DataPoint> dataPoints = new Queue<DataPoint>();
        TimeSpan historyLength = TimeSpan.FromSeconds(2);
        Dictionary<string, Brush> brushes = new Dictionary<string, Brush>();
        public AVPlot()
        {
            InitializeComponent();
            ClipToBounds = true;
            DispatcherTimer dt = new DispatcherTimer(TimeSpan.FromMilliseconds(20), DispatcherPriority.Background, Timer_Tick, Dispatcher);
        }

        private void PruneData()
        {
            lock (dataPoints)
            {
                while (dataPoints.Count > 0 && dataPoints.Peek().Time < CurrentTime - historyLength - TimeSpan.FromSeconds(2))
                    dataPoints.Dequeue();
            }
        }

        public void AddDataPoint(DataPoint dp)
        {
            lock (dataPoints)
                dataPoints.Enqueue(dp);
        }

        private void Timer_Tick(object sender, EventArgs e)
        {
            PruneData();

            if (this.IsVisible)
                InvalidateVisual();
        }

        public void AssocColor(string seriesId, Color b)
        {
            Color bTransparent = b;
            bTransparent.A = 0;

            GradientStopCollection gs = new GradientStopCollection();
            gs.Add(new GradientStop(bTransparent, 0));
            gs.Add(new GradientStop(b, 0.1));
            LinearGradientBrush g = new LinearGradientBrush(gs, new Point(0, 0), new Point(ActualWidth, 0));
            g.MappingMode = BrushMappingMode.Absolute;
            g.Freeze();
            brushes[seriesId] = g;
        }

        protected override void OnRender(DrawingContext dc)
        {
            base.OnRender(dc);

            DataPoint[] localPoints;
            lock (dataPoints)
                localPoints = dataPoints.ToArray();

            var pfs = new Dictionary<string, List<EllipseGeometry>>();

            for (int i = 0; i < localPoints.Length; i++)
            {
                var ptTime = localPoints[i].Time;
                var ptAge = (DateTime.Now - ptTime).TotalSeconds;

                var a = localPoints[i].values["a"];
                var e = localPoints[i].values["e"];
                var p = localPoints[i].values["p"];
                var v = localPoints[i].values["v"];

                var seriesId = "AV";

                var x = ActualWidth * 0.5 + (ActualWidth / 2 - 20) * Math.Min(1, v * (1 / MaxAbsV));
                var y = ActualHeight * 0.5 - (ActualHeight / 2 - 20) * Math.Min(1, a * (1 / MaxAbsA));

                var eg = new EllipseGeometry();
                eg.RadiusX = 5;
                eg.RadiusY = 5;
                eg.Center = new Point(x, y);

                if (!pfs.ContainsKey(seriesId))
                    pfs[seriesId] = new List<EllipseGeometry>();

                pfs[seriesId].Add(eg);

                /*
                seriesId = "EP";

                x = ActualWidth * 0.5 + ActualWidth * Math.Min(e * 0.1,0.45);
                y = ActualHeight * 0.5 - ActualHeight * Math.Min(p * 0.1,0.45);

                eg = new EllipseGeometry();
                eg.RadiusX = 5;
                eg.RadiusY = 5;
                eg.Center = new Point(x, y);

                if (!pfs.ContainsKey(seriesId))
                    pfs[seriesId] = new List<EllipseGeometry>();

                pfs[seriesId].Add(eg);*/

            }

            Pen axisPen = new Pen(Brushes.Black, 1);
            dc.DrawLine(axisPen, new Point(0, (ActualHeight / 2) - 0.5), new Point(ActualWidth, (ActualHeight / 2) - 0.5));
            dc.DrawLine(axisPen, new Point(ActualWidth / 2, 0), new Point((ActualWidth / 2) - 0.5, ActualHeight));
            dc.DrawLine(axisPen, new Point(ActualWidth, (ActualHeight / 2) - 0.5), new Point(ActualWidth - 15, (ActualHeight / 2) - 0.5 - 10));
            dc.DrawLine(axisPen, new Point(ActualWidth, (ActualHeight / 2) - 0.5), new Point(ActualWidth - 15, (ActualHeight / 2) - 0.5 + 10));
            dc.DrawLine(axisPen, new Point((ActualWidth / 2) - 0.5, 0), new Point((ActualWidth / 2) - 0.5 - 10, 15));
            dc.DrawLine(axisPen, new Point((ActualWidth / 2) - 0.5, 0), new Point((ActualWidth / 2) - 0.5 + 10, 15));

            FormattedText t = new FormattedText("Valence", System.Globalization.CultureInfo.CurrentCulture, System.Windows.FlowDirection.LeftToRight, new Typeface("Verdana"), 10, Brushes.Black);
            dc.DrawText(t, new Point(0, ActualHeight / 2));

            FormattedText t2 = new FormattedText("Arousal", System.Globalization.CultureInfo.CurrentCulture, System.Windows.FlowDirection.LeftToRight, new Typeface("Verdana"), 10, Brushes.Black);
            dc.DrawText(t2, new Point(ActualWidth / 2 + 10, ActualHeight - 15));

            Dictionary<string, SolidColorBrush> bs = new Dictionary<string, SolidColorBrush>();
            bs["AV"] = new SolidColorBrush(Color.FromArgb(255, 128, 0, 0));
            bs["EP"] = new SolidColorBrush(Color.FromArgb(255, 0, 128, 0));

            foreach (var kvp in pfs)
            {
                var seriesId = kvp.Key;
                var egs = kvp.Value;

                int i = 0;
                foreach (var eg in egs)
                {
                    var b = bs[seriesId]; //new SolidColorBrush(Color.FromArgb((byte)((i / (double)egs.Count) * 256), 0, 0, 0));
                    var c = b.Color;
                    var d = new SolidColorBrush(Color.FromArgb((byte)((i / (double)egs.Count) * 200), c.R, c.G, c.B));
                    dc.DrawGeometry(d, null, eg);
                    i++;
                }

                if (seriesId == "AV")
                    dc.DrawGeometry(Brushes.Red, new Pen(Brushes.Black, 2), egs.Last());
                else
                    dc.DrawGeometry(Brushes.Green, new Pen(Brushes.Black, 2), egs.Last());

            }
        }


    }
}
