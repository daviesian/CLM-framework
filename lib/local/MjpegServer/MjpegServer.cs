using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace MjpegServer
{
    public class Server
    {
        bool running = false;
        BlockingCollection<byte[]> imgQueue = new BlockingCollection<byte[]>(1);
        int port;

        public Server(int port)
        {
            this.port = port;
        }

        public void Start()
        {
            running = true;
            new Thread(_Start).Start();
        }

        public void Stop()
        {
            running = false;
        }

        private void _Start()
        {
            Thread.CurrentThread.IsBackground = true;
            TcpListener listener = new TcpListener(IPAddress.Any, port);
            listener.Start(0);
            while (running)
            {
                var client = listener.AcceptTcpClient();
                var stream = client.GetStream();

                Console.WriteLine("Accepting request.");

                try
                {
                    while (running)
                    {
                        var imgData = imgQueue.Take();

                        var s = Encoding.ASCII.GetBytes("\r\n--myboundary\r\nContent-Type: image/jpeg\r\nContent-Length: " + imgData.Length + "\r\n\r\n");
                        stream.Write(s, 0, s.Length);
                        stream.Write(imgData, 0, imgData.Length);
                        stream.Flush();
                    }
                }
                catch { }

                client.Close();
            }
        }

        public void EnqueueImage(byte[] image)
        {
            // Drop image if we haven't sent the previous one.
            imgQueue.TryAdd(image);
        }
    }
}
