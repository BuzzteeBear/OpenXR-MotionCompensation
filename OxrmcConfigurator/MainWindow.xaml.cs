using System;
using System.Windows;
using Microsoft.AspNetCore.Components.WebView;
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
    private void Handle_UrlLoading(object sender,
	    UrlLoadingEventArgs urlLoadingEventArgs)
    {
	    if (urlLoadingEventArgs.Url.Host != "0.0.0.0")
	    {
		    urlLoadingEventArgs.UrlLoadingStrategy =
			    UrlLoadingStrategy.OpenExternally;
	    }
    }
}

