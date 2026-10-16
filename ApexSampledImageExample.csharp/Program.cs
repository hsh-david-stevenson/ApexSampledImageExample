using System.CommandLine;
using JawsMako;

namespace ApexSampledImageExample;

internal static class Program
{
    public static int Main(string[] args)
    {
        var commandLine = new CommandLineOptions();
        commandLine.Command.SetAction(result =>
        {
            try
            {
                Renderer.Run(commandLine.Read(result));
                return 0;
            }
            catch (MakoException error)
            {
                Console.Error.WriteLine($"Mako error: {error.m_msg}");
                return 1;
            }
            catch (Exception error)
            {
                Console.Error.WriteLine(error.GetBaseException().Message);
                return 1;
            }
        });
        return commandLine.Command.Parse(args).Invoke();
    }
}
