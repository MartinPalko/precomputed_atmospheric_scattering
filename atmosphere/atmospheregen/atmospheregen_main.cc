#include "atmosphere/atmospheregen/atmospheregen.h"

#include <GL/glew.h>
#include <GL/freeglut.h>

#include "optionparser.h"

#include <memory>
#include <sstream>

namespace commandlineOptionIndex
{
	enum  Type
	{
		unknown = 0,
		help,
		output_directory,
		output_name,
		altitude,
		sun_direction,
		polarization_filter,
		mie_asymmetry,
		mie_scale,
	};
}
const option::Descriptor usage[] =
{
	{
		commandlineOptionIndex::unknown,
		0,
		"",
		"",
		option::Arg::None,
		"USAGE: example [options]\n\n"
		"Options:"
	},
	{
		commandlineOptionIndex::help,
		0,
		"h",
		"help",
		option::Arg::None,
		"--help -h \tPrint usage"
	},
	{
		commandlineOptionIndex::output_directory,
		0,
		"d",
		"output_directory",
		option::Arg::Optional,
		"--output_directory -d \tSpecify output directory."
	},
	{
		commandlineOptionIndex::output_name,
		0,
		"n",
		"output_name",
		option::Arg::Optional,
		"--output_name -n \tSpecify output filename."
	},
	{
		commandlineOptionIndex::altitude,
		0,
		"a",
		"altitude",
		option::Arg::Optional,
		"--altitude -a \tAltitude in KM from earth's surface."
	},
	{
		commandlineOptionIndex::sun_direction,
		0,
		"s",
		"sun_direction",
		option::Arg::Optional,
		"--sun_direction -s \tA vector indicating the sun's direction. Provided as 3 comma seperated values."
	},
	{
		commandlineOptionIndex::polarization_filter,
		0,
		"p",
		"polarization_filter",
		option::Arg::Optional,
		"--polarization_filter -p \tHow much to filter out polarized light."
	},
	{
		commandlineOptionIndex::mie_scale,
		0,
		"m",
		"mie_scale",
		option::Arg::Optional,
		"--mie_scale -m \tScale the number of particles in the atmospehre."
	},
	{
		commandlineOptionIndex::mie_asymmetry,
		0,
		"y",
		"mie_asymmetry",
		option::Arg::Optional,
		"--mie_asymmetry -y \tScale asymmetry of mie scattering."
	},
	{
		commandlineOptionIndex::unknown,
		0,
		"" ,
		""
		,option::Arg::None,
	"\nExamples:\n"
	"  PrecomputedAtmosphericScattering.exe -dC:\\test\n"
	"  PrecomputedAtmosphericScattering.exe --output_directory=C:\\test\n"
	},
	{ 0,0,0,0,0,0 } // Null to end array
};

int main(int argc, char** argv) 
{
	bool error = false;

	argc -= (argc > 0); argv += (argc > 0);
	const option::Stats commandlineStats(usage, argc, argv);
	option::Option* commandlineOptions = new option::Option[commandlineStats.options_max];
	option::Option* buffer = new option::Option[commandlineStats.options_max];
	option::Parser commandlineParser(usage, argc, argv, commandlineOptions, buffer);

	if (commandlineParser.error())
	{
		return 1;
	}

	if (commandlineOptions[commandlineOptionIndex::help] || argc == 0)
	{
		option::printUsage(std::cout, usage);
		return 0;
	}

	atmosphere::AtmosphereGen::Options options;

	// Output directory
	option::Option outputDirectoryOption = commandlineOptions[commandlineOptionIndex::output_directory];
	if (outputDirectoryOption.count() && outputDirectoryOption.arg)
	{
		options.outputDirectory = outputDirectoryOption.arg;

		DWORD dwAttrib = GetFileAttributes(options.outputDirectory);
		if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
		{
			std::cerr << "output_directory specified does not exist!\n";
			error = true;
		}
	}
	else
	{
		std::cerr << "output_directory must be specified!\n";
		error = true;
	}

	// Output name
	option::Option outputNameOption = commandlineOptions[commandlineOptionIndex::output_name];
	if (outputNameOption.count() && outputNameOption.arg)
	{
		options.outputName = outputNameOption.arg;
	}
	else
	{
		std::cerr << "output_name must be specified!\n";
		error = true;
	}

	// Altitude
	option::Option altitudeOption = commandlineOptions[commandlineOptionIndex::altitude];
	if (altitudeOption.count() && altitudeOption.arg)
	{
		options.altitude = std::stof(altitudeOption.arg);
	}

	// Sun direction
	option::Option sunDirectionOption = commandlineOptions[commandlineOptionIndex::sun_direction];
	if (sunDirectionOption.count() && sunDirectionOption.arg)
	{
		std::stringstream strStream(sunDirectionOption.arg);
		std::string segment;

		int i = 0;
		while (std::getline(strStream, segment, ',') && i < 3)
		{
			try
			{
				options.sunDirection[i] = std::stof(segment);
			}
			catch (...)
			{
				std::cerr << "Error parsing argument: " << sunDirectionOption.name << "\n";
				error = true;
			}
			i++;
		}
	}

	// Polarization filter
	option::Option polarizationOption = commandlineOptions[commandlineOptionIndex::polarization_filter];
	if (polarizationOption.count() && polarizationOption.arg)
	{
		try
		{
			options.polarizationFilter = std::stof(polarizationOption.arg);
		}
		catch (...)
		{
			std::cerr << "Error parsing argument: " << polarizationOption.name << "\n";
			error = true;
		}
	}

	// Mie scale
	option::Option mieScaleOption = commandlineOptions[commandlineOptionIndex::mie_scale];
	if (mieScaleOption.count() && mieScaleOption.arg)
	{
		try
		{
			options.mieScale = std::stof(mieScaleOption.arg);
		}
		catch (...)
		{
			std::cerr << "Error parsing argument: " << mieScaleOption.name << "\n";
			error = true;
		}
	}

	if (error)
		return 1;

	options.Print();

	// Initialize OpenGL
	char* fakeArgv[] = { "" };
	int fakeArgc = sizeof(fakeArgv) / sizeof(char*) - 1;
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
	glutInitWindowSize(1024, 1024);
	glutCreateWindow("");
	glewInit();

	// Create and configure atmosphere gen
	atmosphere::AtmosphereGen atmosphereGen(options);

	// Render
	atmosphereGen.RenderAtmosphere();

	return 0;
}
