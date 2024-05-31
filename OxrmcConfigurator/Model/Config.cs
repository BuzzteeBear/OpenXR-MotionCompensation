using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace OxrmcConfigurator.Model;

public enum Tracker : ushort
{
	Error = 0,
	Controller,
	Vive,
	Srs,
	FlyPt,
	Yaw
}
