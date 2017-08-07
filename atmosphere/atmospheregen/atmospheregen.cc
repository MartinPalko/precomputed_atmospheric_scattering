#include "atmosphere/atmospheregen/atmospheregen.h"

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "vec3.h"
#include "mat4.h"
#include "TextureSaver.h"

namespace atmosphere
{
	constexpr double kBottomRadius = 6360000.0;
	constexpr double kTopRadius = 6420000.0;
	constexpr double kRayleigh = 1.24062e-6;
	constexpr double kRayleighScaleHeight = 8000.0;
	constexpr double kMieScaleHeight = 1200.0;
	constexpr double kMieAngstromAlpha = 0.0;
	constexpr double kMieAngstromBeta = 5.328e-3;
	constexpr double kMieSingleScatteringAlbedo = 0.9;
	constexpr double kMiePhaseFunctionG = 0.8;
	constexpr double kGroundAlbedo = 0.1;

	namespace
	{

		constexpr double kPi = 3.1415926;
		constexpr double kSunAngularRadius = 0.00935 / 2.0;
		constexpr double kSunSolidAngle = kPi * kSunAngularRadius * kSunAngularRadius;
		constexpr double kLengthUnitInMeters = 1000.0;

		const char kVertexShader[] = R"(
	#version 330
	uniform mat4 model_from_view;
	uniform mat4 view_from_clip;
	layout(location = 0) in vec4 vertex;
	out vec3 view_ray;
	void main() 
	{
		view_ray = (model_from_view * vec4((view_from_clip * vertex).xyz, 0.0)).xyz;
		gl_Position = vertex;
	})";

#include "atmosphere/atmospheregen/atmospheregen.glsl.inc"

		static std::map<int, AtmosphereGen*> INSTANCES;

	}

	AtmosphereGen::AtmosphereGen(Options options) :
		options(options),
		use_constant_solar_spectrum_(false),
		use_ozone_(true),
		use_combined_textures_(true),
		use_half_precision_(true),
		use_luminance_(PRECOMPUTED),
		do_white_balance_(false),
		show_help_(true),
		program_(0),
		view_distance_meters_(9000.0),
		view_zenith_angle_radians_(1.47),
		view_azimuth_angle_radians_(-0.1),
		sun_zenith_angle_radians_(1.3),
		sun_azimuth_angle_radians_(2.9),
		exposure_(10.0) 
	{
		InitModel();
	}

	/*
	<p>The destructor is even simpler:
	*/

	AtmosphereGen::~AtmosphereGen() {
		glDeleteProgram(program_);
	}

	/*
	<p>The "real" initialization work, which is specific to our atmosphere model,
	is done in the following method. It starts with the creation of an atmosphere
	<code>Model</code> instance, with parameters corresponding to the Earth
	atmosphere:
	*/

	void AtmosphereGen::InitModel()
	{
		// Values from "Reference Solar Spectral Irradiance: ASTM G-173", ETR column
		// (see http://rredc.nrel.gov/solar/spectra/am1.5/ASTMG173/ASTMG173.html),
		// summed and averaged in each bin (e.g. the value for 360nm is the average
		// of the ASTM G-173 values for all wavelengths between 360 and 370nm).
		// Values in W.m^-2.
		constexpr int kLambdaMin = 360;
		constexpr int kLambdaMax = 830;
		constexpr double kSolarIrradiance[48] = {
			1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.6887, 1.61253,
			1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.8298,
			1.8685, 1.8931, 1.85149, 1.8504, 1.8341, 1.8345, 1.8147, 1.78158, 1.7533,
			1.6965, 1.68194, 1.64654, 1.6048, 1.52143, 1.55622, 1.5113, 1.474, 1.4482,
			1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.2367, 1.2082,
			1.18737, 1.14683, 1.12362, 1.1058, 1.07124, 1.04992
		};
		// Values from http://www.iup.uni-bremen.de/gruppen/molspec/databases/
		// referencespectra/o3spectra2011/index.html for 233K, summed and averaged in
		// each bin (e.g. the value for 360nm is the average of the original values
		// for all wavelengths between 360 and 370nm). Values in m^2.
		constexpr double kOzoneCrossSection[48] = {
			1.18e-27, 2.182e-28, 2.818e-28, 6.636e-28, 1.527e-27, 2.763e-27, 5.52e-27,
			8.451e-27, 1.582e-26, 2.316e-26, 3.669e-26, 4.924e-26, 7.752e-26, 9.016e-26,
			1.48e-25, 1.602e-25, 2.139e-25, 2.755e-25, 3.091e-25, 3.5e-25, 4.266e-25,
			4.672e-25, 4.398e-25, 4.701e-25, 5.019e-25, 4.305e-25, 3.74e-25, 3.215e-25,
			2.662e-25, 2.238e-25, 1.852e-25, 1.473e-25, 1.209e-25, 9.423e-26, 7.455e-26,
			6.566e-26, 5.105e-26, 4.15e-26, 4.228e-26, 3.237e-26, 2.451e-26, 2.801e-26,
			2.534e-26, 1.624e-26, 1.465e-26, 2.078e-26, 1.383e-26, 7.105e-27
		};
		// From https://en.wikipedia.org/wiki/Dobson_unit, in molecules.m^-2.
		constexpr double kDobsonUnit = 2.687e20;
		// Maximum number density of ozone molecules, in m^-3 (computed so at to get
		// 300 Dobson units of ozone - for this we divide 300 DU by the integral of
		// the ozone density profile defined below, which is equal to 15km).
		constexpr double kMaxOzoneNumberDensity = 300.0 * kDobsonUnit / 15000.0;
		// Wavelength independent solar irradiance "spectrum" (not physically
		// realistic, but was used in the original implementation).
		constexpr double kConstantSolarIrradiance = 1.5;
		const double max_sun_zenith_angle = (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;

		DensityProfileLayer
			rayleigh_layer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
		DensityProfileLayer mie_layer(0.0, 1.0, -1.0 / kMieScaleHeight, 0.0, 0.0);
		// Density profile increasing linearly from 0 to 1 between 10 and 25km, and
		// decreasing linearly from 1 to 0 between 25 and 40km. This is an approximate
		// profile from http://www.kln.ac.lk/science/Chemistry/Teaching_Resources/
		// Documents/Introduction%20to%20atmospheric%20chemistry.pdf (page 10).
		std::vector<DensityProfileLayer> ozone_density;
		ozone_density.push_back(
			DensityProfileLayer(25000.0, 0.0, 0.0, 1.0 / 15000.0, -2.0 / 3.0));
		ozone_density.push_back(
			DensityProfileLayer(0.0, 0.0, 0.0, -1.0 / 15000.0, 8.0 / 3.0));

		std::vector<double> wavelengths;
		std::vector<double> solar_irradiance;
		std::vector<double> rayleigh_scattering;
		std::vector<double> mie_scattering;
		std::vector<double> mie_extinction;
		std::vector<double> absorption_extinction;
		std::vector<double> ground_albedo;
		for (int l = kLambdaMin; l <= kLambdaMax; l += 10)
		{
			double lambda = static_cast<double>(l) * 1e-3;  // micro-meters
			double mie =
				(kMieAngstromBeta * options.mieScale) / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
			wavelengths.push_back(l);
			if (use_constant_solar_spectrum_)
			{
				solar_irradiance.push_back(kConstantSolarIrradiance);
			}
			else
			{
				solar_irradiance.push_back(kSolarIrradiance[(l - kLambdaMin) / 10]);
			}
			rayleigh_scattering.push_back(kRayleigh * pow(lambda, -4));
			mie_scattering.push_back(mie * kMieSingleScatteringAlbedo);
			mie_extinction.push_back(mie);
			absorption_extinction.push_back(use_ozone_ ?
				kMaxOzoneNumberDensity * kOzoneCrossSection[(l - kLambdaMin) / 10] :
				0.0);
			ground_albedo.push_back(kGroundAlbedo);
		}

		model_.reset(new Model(wavelengths, solar_irradiance, kSunAngularRadius,
			kBottomRadius, kTopRadius, {rayleigh_layer}, rayleigh_scattering,
			{mie_layer}, mie_scattering, mie_extinction, kMiePhaseFunctionG,
			ozone_density, absorption_extinction, ground_albedo, max_sun_zenith_angle,
			kLengthUnitInMeters, use_luminance_ == PRECOMPUTED ? 15 : 3,
			use_combined_textures_, use_half_precision_));
		model_->Init();

		/*
		<p>Then, it creates and compiles the vertex and fragment shaders used to render
		our demo scene, and link them with the <code>Model</code>'s atmosphere shader
		to get the final scene rendering program:
		*/

		GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		const char* const vertex_shader_source = kVertexShader;
		glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
		glCompileShader(vertex_shader);

		const std::string fragment_shader_str =
			"#version 330\n" +
			std::string(use_luminance_ != NONE ? "#define USE_LUMINANCE\n" : "") +
			atmospheregen_glsl;
		const char* fragment_shader_source = fragment_shader_str.c_str();
		GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
		glCompileShader(fragment_shader);

		if (program_ != 0)
		{
			glDeleteProgram(program_);
		}
		program_ = glCreateProgram();
		glAttachShader(program_, vertex_shader);
		glAttachShader(program_, fragment_shader);
		glAttachShader(program_, model_->GetShader());
		glLinkProgram(program_);
		glDetachShader(program_, vertex_shader);
		glDetachShader(program_, fragment_shader);
		glDetachShader(program_, model_->GetShader());
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		/*
		<p>Finally, it sets the uniforms of this program that can be set once and for
		all (in our case this includes the <code>Model</code>'s texture uniforms,
		because our demo app does not have any texture of its own):
		*/

		glUseProgram(program_);
		model_->SetProgramUniforms(program_, 0, 1, 2, 3);
		double white_point_r = 1.0;
		double white_point_g = 1.0;
		double white_point_b = 1.0;
		if (do_white_balance_) {
			Model::ConvertSpectrumToLinearSrgb(wavelengths, solar_irradiance,
				&white_point_r, &white_point_g, &white_point_b);
			double white_point = (white_point_r + white_point_g + white_point_b) / 3.0;
			white_point_r /= white_point;
			white_point_g /= white_point;
			white_point_b /= white_point;
		}
		glUniform3f(glGetUniformLocation(program_, "white_point"),
			white_point_r, white_point_g, white_point_b);
		glUniform3f(glGetUniformLocation(program_, "earth_center"),
			0.0, 0.0, -kBottomRadius / kLengthUnitInMeters);
		glUniform2f(glGetUniformLocation(program_, "sun_size"),
			tan(kSunAngularRadius),
			cos(kSunAngularRadius));
	}

	void AtmosphereGen::RenderAtmosphere()
	{
		std::cout << "rendering cubemap... " << std::endl;

		const unsigned int cubemap_resolution = 1024;

		GLuint fbo;
		glGenFramebuffersEXT(1, &fbo);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

		// Allocate cubemap
		GLuint cubeTexture;
		glGenTextures(1, &cubeTexture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTexture);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB32F_ARB, cubemap_resolution, cubemap_resolution, 0, GL_RGB, GL_FLOAT, NULL);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB32F_ARB, cubemap_resolution, cubemap_resolution, 0, GL_RGB, GL_FLOAT, NULL);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB32F_ARB, cubemap_resolution, cubemap_resolution, 0, GL_RGB, GL_FLOAT, NULL);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB32F_ARB, cubemap_resolution, cubemap_resolution, 0, GL_RGB, GL_FLOAT, NULL);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB32F_ARB, cubemap_resolution, cubemap_resolution, 0, GL_RGB, GL_FLOAT, NULL);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB32F_ARB, cubemap_resolution, cubemap_resolution, 0, GL_RGB, GL_FLOAT, NULL);

		// Set program
		glViewport(0, 0, cubemap_resolution, cubemap_resolution);

		// Unit vectors of the camera frame, expressed in world space.
		float cos_z = cos(view_zenith_angle_radians_);
		float sin_z = sin(view_zenith_angle_radians_);
		float cos_a = cos(view_azimuth_angle_radians_);
		float sin_a = sin(view_azimuth_angle_radians_);
		float ux[3] = { -sin_a, cos_a, 0.0 };
		float uy[3] = { -cos_z * cos_a, -cos_z * sin_a, sin_z };
		float uz[3] = { sin_z * cos_a, sin_z * sin_a, cos_z };
		float l = view_distance_meters_ / kLengthUnitInMeters;

		// Transform matrix from camera frame to world space (i.e. the inverse of a
		// GL_MODELVIEW matrix).
		float model_from_view[16] = {
			ux[0], uy[0], uz[0], uz[0] * l,
			ux[1], uy[1], uz[1], uz[1] * l,
			ux[2], uy[2], uz[2], uz[2] * l,
			0.0, 0.0, 0.0, 1.0
		};

		const float kFovY = 90.0f / 180.0f * kPi;
		const float kTanFovY = tan(kFovY / 2.0f);
		float aspect_ratio = 1.0f;

		// Transform matrix from clip space to camera space (i.e. the inverse of a
		// GL_PROJECTION matrix).
		float view_from_clip[16] = {
			kTanFovY * aspect_ratio, 0.0, 0.0, 0.0,
			0.0, kTanFovY, 0.0, 0.0,
			0.0, 0.0, 0.0, -1.0,
			0.0, 0.0, 1.0, 1.0
		};
		glUniformMatrix4fv(glGetUniformLocation(program_, "view_from_clip"), 1, true, view_from_clip);

		glUniform1f(glGetUniformLocation(program_, "exposure"), use_luminance_ != NONE ? exposure_ * 1e-5 : exposure_);
		glUniform3f(glGetUniformLocation(program_, "sun_direction"), options.sunDirection[0], options.sunDirection[1], options.sunDirection[2]);
		
		vec3d position = vec3d(0, 0, options.altitude);
		glUniform3f(glGetUniformLocation(program_, "camera"), position.x, position.y, position.z);

		float h = position.length() - kBottomRadius;
		mat4f proj = mat4f::perspectiveProjection(90, 1, 10, 1e5 * h);
		mat4f iproj = proj.inverse();

		// Render each face of the cubemap
		for (int i = 0; i < 6; i++)
		{
			const GLenum face = GL_TEXTURE_BINDING_CUBE_MAP + 1 + i;
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, face, cubeTexture, 0);

			glClearColor(0.0, 1.0, 1.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);

			mat4d view = mat4d();
			switch (i + GL_TEXTURE_BINDING_CUBE_MAP + 1)
			{
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
				view = mat4d::rotate(vec3d(270, 180, 0));
				break;
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
				view = mat4d::rotate(vec3d(-270, 180, 0));
				break;
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
				view = mat4d::rotate(vec3d(0, 90, 0));
				break;
			case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
				view = mat4d::rotate(vec3d(0, -90, 0));
				break;
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
				view = mat4d::rotate(vec3d(0, 0, 180));
				break;
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
				view = mat4d::rotate(vec3d(0, 180, 0));
				break;
			default:
				continue;
				break;
			}

			view = view * mat4d::translate(position);
			mat4d iview = view.inverse();
			mat4f iviewf = mat4f(iview[0][0], iview[0][1], iview[0][2], iview[0][3],
				iview[1][0], iview[1][1], iview[1][2], iview[1][3],
				iview[2][0], iview[2][1], iview[2][2], iview[2][3],
				iview[3][0], iview[3][1], iview[3][2], iview[3][3]);

			glUniformMatrix4fv(glGetUniformLocation(program_, "model_from_view"), 1, true, iviewf.coefficients());

			// Draw quad
			glBegin(GL_TRIANGLE_STRIP);
			glVertex4f(-1.0, -1.0, 0.0, 1.0);
			glVertex4f(+1.0, -1.0, 0.0, 1.0);
			glVertex4f(-1.0, +1.0, 0.0, 1.0);
			glVertex4f(+1.0, +1.0, 0.0, 1.0);
			glEnd();
		}

		std::cout << "writing cubemap..." << std::endl;

		glFinish();
		SaveTextureToTiff((std::string(options.outputDirectory) + "\\" + options.outputName + ".tif").c_str(), cubeTexture, GL_TEXTURE_CUBE_MAP, false, cubemap_resolution, cubemap_resolution);

		glDeleteTextures(1, &cubeTexture);
	}
}