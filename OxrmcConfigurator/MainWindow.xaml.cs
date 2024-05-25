using System;
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
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Serilog;
using MudBlazor.Services;
using MudExtensions.Services;
using OxrmcConfigurator.Model;

namespace OxrmcConfigurator;
public partial class MainWindow : Window
{
    public MainWindow()
    {
        using var serilog = new LoggerConfiguration()
            .WriteTo.Console()
            .CreateLogger();
        Log.Logger = serilog;
        var loggerFactory = new LoggerFactory()
	        .AddSerilog(serilog);
        Microsoft.Extensions.Logging.ILogger logger = loggerFactory.CreateLogger("Logger");

		AppDomain.CurrentDomain.UnhandledException += (sender, error) =>
        {
#if DEBUG
            MessageBox.Show(error.ExceptionObject.ToString(), caption: "Error");
#else
            MessageBox.Show(text: "An error has occurred.", caption: "Error");
#endif

            // Log the error information (error.ExceptionObject)
        };

        var serviceCollection = new ServiceCollection();
        serviceCollection.AddWpfBlazorWebView();
        serviceCollection.AddMudServices();
        serviceCollection.AddMudExtensions();
		serviceCollection.AddSingleton<ConfigService>();
        
		Resources.Add("services", serviceCollection.BuildServiceProvider());
    }
}
