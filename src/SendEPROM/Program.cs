using System;
using System.IO;
using System.IO.Ports;
using System.CommandLine;
using System.Threading.Tasks;
using System.Collections.Generic;

namespace SendEPROM
{
	public class Program
    {
        private static readonly List<int> Sizes = 
        [
                   0,
              1*1024,
              2*1024,
              4*1024,
              8*1024,
             16*1024,
             32*1024,
             64*1024,
            128*1024,
            256*1024,
            512*1024,
           1024*1024,
        ];

        public static async Task<int> Main(string[] args)
        {
            var portOption = new Option<string>(["--port", "-p"], description: "COM port on which to send the file", getDefaultValue: () => "COM3");
            var baudOption = new Option<int>   (["--baud", "-b"], description: "Baud rate at which to send",         getDefaultValue: () => 115200);
            var modeOption = new Option<byte>  (["--type", "-t"], description: "EPROM type\n" +
                                                                               " 1 = 2708\n" +
                                                                               " 2 = 2716\n" +
                                                                               " 3 = 2732\n" +
                                                                               " 4 = 2764\n" +
                                                                               " 5 = 27128\n" +
                                                                               " 6 = 27256\n" +
                                                                               " 7 = 27512\n" +
                                                                               " 8 = 27010\n" +
                                                                               " 9 = 27020\n" +
                                                                               "10 = 27040\n" + 
                                                                               "11 = 27080",                         getDefaultValue: () => 10);
            var skipOption = new Option<int>   (["--skip", "-s"], description: "Skip N bytes at start of file",      getDefaultValue: () => 0);
            var lynxOption = new Option<bool>  (["--lynx", "-l"], description: "Atari Lynx dev cart mode",           getDefaultValue: () => false);

            var fileArgument = new Argument<FileInfo>("file", description: "File to send");

            var rootCmd = new RootCommand("Send a binary image to the EPROM emulator")
            {
                portOption,
                baudOption,
                modeOption,
                skipOption,
                lynxOption,
                fileArgument,
            };

            rootCmd.AddValidator(validate =>
            {
                byte mode = validate.GetValueForOption(modeOption);
                if(mode < 1 || mode > 11)
                {
                    validate.ErrorMessage = "Invalid mode specified";
                    return;
                }

                FileInfo file = validate.GetValueForArgument(fileArgument);
                if(file == null || !file.Exists)
                    validate.ErrorMessage = "File not specified";
                else if(file.Length > Sizes[mode])
                    validate.ErrorMessage = "File is too large for selected EPROM mode";
            });

            rootCmd.SetHandler(
                (port, baud, mode, skip, lynx, file) =>
                {
                    SendData(port, baud, mode, skip, lynx, file);
                },
                portOption, baudOption, modeOption, skipOption, lynxOption, fileArgument);

            return await rootCmd.InvokeAsync(args);
        }

        static void SendData(string port, int baud, byte mode, int skip, bool lynx, FileInfo file)
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
                Console.WriteLine($"Sending mode {mode}, Lynx {lynx}");
                byte[] modeBuff = { mode, lynx ? (byte)1 : (byte)0 };
                sp.Write(modeBuff, 0, modeBuff.Length);
            }

            Console.WriteLine($"Sending {buff.Length} bytes, skipping {skip} bytes");
            sp.Write(buff, skip, buff.Length - skip);
            sp.Close();
            Console.WriteLine("Done!");
        }
    }
}