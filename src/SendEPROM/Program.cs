using System;
using System.IO;
using System.IO.Ports;
using System.CommandLine;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Security.Cryptography;

namespace SendEPROM
{
	public class Program
    {
        private static readonly List<int> Sizes = 
        [
             0, 
             32*1024,
            128*1024,
            256*1024
        ];

        public static async Task<int> Main(string[] args)
        {
            var portOption = new Option<string>(["--port", "-p"], description: "COM port on which to send the file",              getDefaultValue: () => "COM3");
            var baudOption = new Option<int>   (["--baud", "-b"], description: "Baud rate at which to send",                      getDefaultValue: () => 115200);
            var modeOption = new Option<byte>  (["--mode", "-m"], description: "EPROM mode (1 = 27C256, 2 = 27C010, 3 = 27C020)", getDefaultValue: () => 3);
            var skipOption = new Option<int>   (["--skip", "-s"], description: "Skip N bytes at start of file",                   getDefaultValue: () => 0);

            var fileArgument = new Argument<FileInfo>("file", description: "File to send");

            var rootCmd = new RootCommand("Send a binary image to the EPROM emulator")
            {
                portOption,
                baudOption,
                modeOption,
                skipOption,
                fileArgument
            };

            rootCmd.AddValidator(validate =>
            {
                byte mode = validate.GetValueForOption(modeOption);
                if(mode < 1 || mode > 3)
                {
                    validate.ErrorMessage = "Invalid mode";
                    return;
                }

                FileInfo file = validate.GetValueForArgument(fileArgument);
                if(file.Length > Sizes[mode])
                    validate.ErrorMessage = "File is too large for selected EPROM mode";
            });

            rootCmd.SetHandler(
                (port, baud, mode, skip, file) =>
                {
                    SendData(port, baud, mode, skip, file);
                },
                portOption, baudOption, modeOption, skipOption, fileArgument);

            return await rootCmd.InvokeAsync(args);
        }

        static void SendData(string port, int baud, byte mode, int skip, FileInfo file)
        {
			Console.WriteLine($"Reading {file.FullName}");
			byte[] buff = File.ReadAllBytes(file.FullName);

            Console.WriteLine($"Opening {port} at {baud}");
			var sp = new SerialPort(port)
			{
				BaudRate = baud
			};
			sp.Open();

            if(mode != 0)
            {
                Console.WriteLine($"Sending mode {mode}");
                byte[] modeBuff = { mode };
                sp.Write(modeBuff, 0, 1);
            }

            Console.WriteLine($"Sending {buff.Length} bytes, skipping {skip} bytes");
            sp.Write(buff, skip, buff.Length - skip);
            sp.Close();
            Console.WriteLine("Done!");
        }
    }
}