/* Generador de imagenes virtuales usando interpolador Patched */

#include <iostream>
#include "itkImage.h"
#include "itkTimeProbesCollectorBase.h"
#include "itkImageFileReader.h"
#include "itkResampleImageFilter.h"
#include "itkSimilarity3DTransform.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkFlipImageFilter.h"
#include "itkImageFileWriter.h"
#include "../itkPatchedRayCastInterpolateImageFunction.h"
// funcion de menu principal 


void menu(){
	std::cout<<"Generador de imagenes virtuales : Parametros necesitados"<< std::endl;
}

int main(int argc, char *argv[]){

	//variable definition
	char *input_name = NULL;		//input volume
        char *output_name = NULL;		//virtual image

	float scd = 1000.0			//distance from source to isocenter
	
	float cx = 0.;				//virtual image isocenter in x
	float cy = 0.;				//virtual image isocenter in y
	float cz = 0.;				//virtual image isocenter in z
	
	float threshold = 0.;			//virtual image threshold

	int dx = 512;				//pixels number virtual image in x
	int dy = 512;				//pixels number virtual image in y
	
	float im_sx = 0.51;			//virtual image spacing in x
	float im_st = 0.51;			//virtual image spacing in y
	
	float o2Dx;				//virtual image origin in x
	float o2Dy;				//virtual image origin in y

	bool ok;				//sanity of parameters
	float rprojection = 0.; 		//projection angle: AP view by default
	bool verbose = false;			//information flag
	
	itk::TimeProbesCollectorBase timer;	//Time Record


	//initialization variables
	//In order to evaluate every parameter the condition flag is always false
	//so whatever quantity of parameters the last parameter put on true the flag
	//condition
	//
	//So, the first argument argv[0] is the executable command
	while(argc > 1){
		ok = false;

		if ((ok == false) && (strcmp(argv[1], "-v") == 0))
		{
			argc--; argv++;
			ok = true;
			verbose = true;
		}
		
		if ((ok == false) && (strcmp(argv[1], "-rp") == 0))
		{
			argc--; argv++;
			ok = true;
			rprojection=atof(argv[1]);
			argc--; argv++;
		}

		if ((ok == false) && (strcmp(argv[1], "-iso") == 0))
		{
			argc--; argv++;
			ok = true;
			cx=atof(argv[1]);
			argc--; argv++;
			cy=atof(argv[1]);
			argc--; argv++;
			cz=atof(argv[1]);
			argc--; argv++;
			customized_iso = true;
		}
		
		if ((ok == false) && (strcmp(argv[1], "-threshold") == 0))
		{
			argc--; argv++;
			ok = true;
			threshold=atof(argv[1]);
			argc--; argv++;
		}

		if ((ok == false) && (strcmp(argv[1], "-res") == 0))
		{
			argc--; argv++;
			ok = true;
			im_sx=atof(argv[1]);
			argc--; argv++;
			im_sy=atof(argv[1]);
			argc--; argv++;
		}

		if ((ok == false) && (strcmp(argv[1], "-size") == 0))
		{
			argc--; argv++;
			ok = true;
			dx=atoi(argv[1]);
			argc--; argv++;
			dy=atoi(argv[1]);
			argc--; argv++;
		}
		
		if ((ok == false) && (strcmp(argv[1], "-scd") == 0))
		{
			argc--; argv++;
			ok = true;
			scd = atof(argv[1]);
			argc--; argv++;
		}

	
		if ((ok == false) && (strcmp(argv[1], "-o") == 0))
		{
			argc--; argv++;
			ok = true;
			output_name = argv[1];
			argc--; argv++;
		}

		if (ok == false) 
		{

			if (input_name == NULL) 
			{
				input_name = argv[1];
				argc--;
				argv++;
			}
			else 
				std::cerr << "ERROR: Can not parse the image " << argv[1] << std::endl;

		}
	

	}
	if (verbose) 
	{
		if (input_name)  std::cout << "Input image: "  << input_name  << std::endl;
		if (output_name) std::cout << "Output image: " << output_name << std::endl;
	}


	//function con las terceras parte
	menu();	
	const unsigned int Dimension = 3;

	//tipo de pixel by default para las imagenes
	typedef short int InputPixelType;
	typedef unsigned char OutputPixelType;

	//typedef itk::Image<short int, Dimensions> FixedImageType;
	typedef itk::Image<InputPixelType, Dimension> MovingImageType;
	typedef itk::Image<OutputPixelType, Dimension> OutputImageType;

	MovingImageType::Pointer image;

	// Reading the Moving Image
	if (input_name) 
	{
		timer.Start("Loading Input Image");
		typedef itk::ImageFileReader< MovingImageType >  ReaderType;
		ReaderType::Pointer reader = ReaderType::New();
		reader->SetFileName( input_name );

		try { 
			reader->Update();
		} 

		catch( itk::ExceptionObject & err ) 
		{ 
			std::cerr << "ERROR: ExceptionObject caught !" << std::endl; 
			std::cerr << err << std::endl; 
			return EXIT_FAILURE;
		} 

		image = reader->GetOutput();
		timer.Stop("Loading Input Image");
	}
	else 
	{
		std::cerr << "Input image file missing !" << std::endl;
		return EXIT_FAILURE;
	}

	if (verbose) 
	{
		unsigned int i;
		const itk::Vector<double, 3> spacing = image->GetSpacing();  
		std::cout << std::endl << "Input ";

		MovingImageType::RegionType region = image->GetBufferedRegion();
		region.Print(std::cout);

		std::cout << "  Resolution: [";
		for (i=0; i<Dimension; i++) 
		{
			std::cout << spacing[i];
			if (i < Dimension-1) std::cout << ", ";
		}
		std::cout << "]" << std::endl;

		const itk::Point<double, 3> origin = image->GetOrigin();
		std::cout << "  Origin: [";
		for (i=0; i<Dimension; i++) 
		{
			std::cout << origin[i];
			if (i < Dimension-1) std::cout << ", ";
		}
		std::cout << "]" << std::endl<< std::endl;
		std::cout<< "Isocenter: " <<isocenter[0] <<", "<< isocenter[1] << ", "<< isocenter[2] << std::endl;
		std::cout << "Transform: " << transform << std::endl;
	}

	//The resample filter enables coordinates for each of the pixels in DRR image.
	//these coordinates are used by interpolator to determine the equatio of each
	//ray which trough the volume

	typedef itk::ResampleImageFilter<MovingImageType, MovingImageType> FilterType;

	FilterType::Pointer filter = FilterType::New();
	filter->SetInput(image);
	filter->SetDefaultPixelValue(0);

	typedef itk::Similarity3DTransform< double > TransformType;
	TransformType::Pointer transform = TransformType::New();
	
	//constant for casting degrees into radians format of rotation projection
	const double dtr = ( atan(1.0) * 4.0 ) / 180.0;

	//Read image properties in order to build our isocenter
	MovingImageType::PointType imOrigin = image->GetOrigin();
	MovingImageType::SpacingType imRes = image->GetSpacing();

	typedef MovingImageType::RegionType InputImageRegionType;
	typedef MovingImageType::SizeType InputImageSizeType;

	InputImageRegionType imRegion = image->GetBufferedRegion();
	InputImageSizeType imSize = imRegion->GetSize();

	TransformType::InputPointType isocenter;
	
	//Consider the center of the volume

	if (customized_iso)
	{
		// Isocenter location given by the user.
		isocenter[0] = imOrigin[0] + imRes[0] * cx; 
		isocenter[1] = imOrigin[1] + imRes[1] * cy; 
		isocenter[2] = imOrigin[2] + imRes[2] * cz;
	}
	else
	{
		// Set the center of the image as the isocenter.
		isocenter[0] = imOrigin[0] + imRes[0] * static_cast<double>( imSize[0] ) / 2.0; 
		isocenter[1] = imOrigin[1] + imRes[1] * static_cast<double>( imSize[1] ) / 2.0; 
		isocenter[2] = imOrigin[2] + imRes[2] * static_cast<double>( imSize[2] ) / 2.0;
	}
	
	transform->setCenter(isocenter);
	
	//Instance of the interpolator
	typedef itk::PatchedRayCastInterpolateImageFunction<MovingImageType, double> InterpolatorType;
	InterpolatorType::Pointer interpolator = InterpolatorType::New();
	
	//REMEMBER: It is according to Interpolator Class
	//take care about this
	interpolator->SetThreshold(interpolator);
	inteporlator->SetProjectionAngle( dtr * rprojection );
	interpolator->SetTransform(transform);
	interpolator->Initialize();
	
	//insert the interpolator into the filter
	filter->SetInterpolator(interpolator);
	
	//Setting properties of fixed image
	
	MovingImageType::SizeType size;
	double spacing[Dimension];
	
	//Number of pixels of Fixed Image
	size[0] = dx;
	size[1] = dy;
	size[2] = 1;
	
	//Resolution of Fixed Image
	spacing[0] = im_sx;
	spacing[1] = im_sy;
	spacing[2] = 1.0;
	
	// Compute the origin (in mm) of the 2D Image
	origin[0] = - im_sx * o2Dx; 
	origin[1] = - im_sy * o2Dy;
	origin[2] = - scd;

	//set properties of the virtual image
	filter->SetSize(size);
	filter->SetOutSpacing(spacing);
	filter->SetOutputOrigin(origin);

	//Virtual Image Properties information
	if(verbose)
	{
		std::cout << "Output image size: " 
			<< size[0] << ", " 
			<< size[1] << ", " 
			<< size[2] << std::endl;

		std::cout << "Output image spacing: " 
			<< spacing[0] << ", " 
			<< spacing[1] << ", " 
			<< spacing[2] << std::endl;
		
		std::cout << "Output image origin: "
			<< origin[0] << ", " 
			<< origin[1] << ", " 
			<< origin[2] << std::endl;
	}
	
	//applying the resample filter (generation)
	timer.Start("DRR generation");
	filter->Update();
	timer.Stop("DRR generation");
	
	if (output_name) 
	{

		// The output of the filter can then be passed to a writer to
		// save the DRR image to a file.

		typedef itk::RescaleIntensityImageFilter< 
			InputImageType, OutputImageType > RescaleFilterType;
		RescaleFilterType::Pointer rescaler = RescaleFilterType::New();
		rescaler->SetOutputMinimum(   0 );
		rescaler->SetOutputMaximum( 255 );
		rescaler->SetInput( filter->GetOutput() );

		timer.Start("DRR post-processing");
		rescaler->Update();

		// Out of some reason, the computed projection is upsided-down.
		// Here we use a FilpImageFilter to flip the images in y direction.
		typedef itk::FlipImageFilter< OutputImageType > FlipFilterType;
		FlipFilterType::Pointer flipFilter = FlipFilterType::New();

		typedef FlipFilterType::FlipAxesArrayType FlipAxesArrayType;
		FlipAxesArrayType flipArray;
		flipArray[0] = 0;
		flipArray[1] = 1;

		flipFilter->SetFlipAxes( flipArray );
		flipFilter->SetInput( rescaler->GetOutput() );
		flipFilter->Update();

		timer.Stop("DRR post-processing");

		typedef itk::ImageFileWriter< OutputImageType >  WriterType;
		WriterType::Pointer writer = WriterType::New();

		// Now we are ready to write the projection image.
		writer->SetFileName( output_name );
		writer->SetInput( flipFilter->GetOutput() );

		try 
		{ 
			std::cout << "Writing image: " << output_name << std::endl;
			writer->Update();
		} 
		catch( itk::ExceptionObject & err ) 
		{ 
			std::cerr << "ERROR: ExceptionObject caught !" << std::endl; 
			std::cerr << err << std::endl; 
		} 
	}
	
	timer.Report();
	return 0;
}
