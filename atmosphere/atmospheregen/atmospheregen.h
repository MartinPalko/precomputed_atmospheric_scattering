#pragma once

#include <memory>
#include <iostream>

#include "atmosphere/model.h"

namespace atmosphere
{
	class AtmosphereGen
	{
	public:

		struct Options
		{
			// Output options
			const char* outputDirectory;
			const char* outputName;
			bool outputLookupTextures;
			bool outputCubemap;
			int cubemapResolution;

			// Render options
			float altitude;
			float sunDirection[3];
			float polarizationFilter;
			float mieScale;

			Options()
				: outputDirectory("")
				, outputName("")
				, outputLookupTextures(false)
				, outputCubemap(false)
				, cubemapResolution(1024)
				, altitude(0.1f)
				, sunDirection{ 1.0, 0.0f, 0.0f }
				, polarizationFilter(0.0f)
				, mieScale(1.0f)
			{
			}

			void Print()
			{
				std::cout << "outputDirectory: " << outputDirectory << "\n";
				std::cout << "outputName: " << outputName << "\n";
				std::cout << "outputLookupTextures: " << outputLookupTextures << "\n";
				std::cout << "outputCubemap: " << outputCubemap << "\n";
				std::cout << "cubemapResolution: " << cubemapResolution << "\n";
				std::cout << "altitude: " << altitude << "\n";
				std::cout << "sunDirection: " << sunDirection[0] << "," << sunDirection[1] << "," << sunDirection[2] << "\n";
				std::cout << "polarizationFilter: " << polarizationFilter << "\n";
				std::cout << "mieScale: " << mieScale << "\n";
			}
		};

		AtmosphereGen(Options options);
		~AtmosphereGen();

		void RenderAtmosphere();

	private:
		enum Luminance
		{
			// Render the spectral radiance at kLambdaR, kLambdaG, kLambdaB.
			NONE,
			// Render the sRGB luminance, using an approximate (on the fly) conversion
			// from 3 spectral radiance values only (see section 14.3 in <a href=
			// "https://arxiv.org/pdf/1612.04336.pdf">A Qualitative and Quantitative
			//  Evaluation of 8 Clear Sky Models</a>).
			APPROXIMATE,
			// Render the sRGB luminance, precomputed from 15 spectral radiance values
			// (see section 4.4 in <a href=
			// "http://www.oskee.wz.cz/stranka/uploads/SCCG10ElekKmoch.pdf">Real-time
			//  Spectral Scattering in Large-scale Natural Participating Media</a>).
			PRECOMPUTED
		};

		void InitModel();

		Options options;

		bool use_constant_solar_spectrum_;
		bool use_ozone_;
		bool use_combined_textures_;
		bool use_half_precision_;
		Luminance use_luminance_;
		bool do_white_balance_;
		bool show_help_;

		std::unique_ptr<Model> model_;
		unsigned int program_;

		double view_distance_meters_;
		double view_zenith_angle_radians_;
		double view_azimuth_angle_radians_;
		double sun_zenith_angle_radians_;
		double sun_azimuth_angle_radians_;
		double exposure_;

		int previous_mouse_x_;
		int previous_mouse_y_;
		bool is_ctrl_key_pressed_;
	};
}