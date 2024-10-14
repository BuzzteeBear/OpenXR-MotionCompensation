using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media.Imaging;

namespace OxrmcDashboard
{
	/// <summary>
	/// Interaction logic for MainWindow.xaml
	/// </summary>
	public partial class MainWindow : Window
	{
		private BitmapImage _oxrmcLogo;

		public MainWindow()
		{
			_oxrmcLogo = LoadBitmap("oxrmc_grey.png");
			InitializeComponent();
		} 
		private BitmapImage LoadBitmap(string fileName)
		{
			BitmapImage bitmap = new BitmapImage();
			bitmap.BeginInit();
			bitmap.DecodePixelWidth = 600;
			bitmap.UriSource = new Uri($"pack://application:,,,/OxrmcDashboardLibrary;component/img/{fileName}");
			bitmap.EndInit();
			return bitmap;
		}
		private void LoadLogo(object sender, RoutedEventArgs e)
		{
				OxrmcLoge.Source = _oxrmcLogo;
		}
	}
}