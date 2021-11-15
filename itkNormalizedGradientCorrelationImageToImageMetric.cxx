
#ifndef __itkNormalizedGradientCorrelationImageToImageMetric_txx
#define __itkNormalizedGradientCorrelationImageToImageMetric_txx

#include "itkNormalizedGradientCorrelationImageToImageMetric.h"
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkNumericTraits.h>



namespace itk
{

	/**
	 * Constructor
	 */
	template <class TFixedImage, class TMovingImage>
		NormalizedGradientCorrelationImageToImageMetric<TFixedImage,TMovingImage>
		::NormalizedGradientCorrelationImageToImageMetric()
		{
			m_DerivativeDelta = 0.001;
			m_MaxDimension = FixedImageDimension;
		}


	/**
	 * Initialize
	 */
	template <class TFixedImage, class TMovingImage>
		void
		NormalizedGradientCorrelationImageToImageMetric<TFixedImage,TMovingImage>
		::Initialize(void)
		{
			// Initialise the base class
			Superclass::Initialize();

			SizeType size = this->GetFixedImageRegion().GetSize();
			for( unsigned int dim=0; dim < FixedImageDimension; dim++ )
			{
				if( size[dim] < 2 )
				{
					m_MaxDimension = dim;
					break;
				}
			}

			for (unsigned int dim=0; dim<m_MaxDimension; dim++)
			{
				m_SobelOperators[dim].SetRadius( 1 );
				m_SobelOperators[dim].SetDirection( dim );
				m_SobelOperators[dim].CreateDirectional();

				m_FixedSobelFilters[dim] = SobelFilterType::New();
				m_FixedSobelFilters[dim]->OverrideBoundaryCondition(
						&m_FixedBoundaryCondition );
				m_FixedSobelFilters[dim]->SetOperator( m_SobelOperators[dim] );
				m_FixedSobelFilters[dim]->SetInput( this->GetFixedImage() );
				m_FixedSobelFilters[dim]->GetOutput()->SetRequestedRegion( this->GetFixedImageRegion() );
			}
			/*
			   std::cout<<"Direction Before Moving 3D: "<<std::endl;
			   std::cout<<this->m_MovingImage->GetDirection()<<std::endl;
			   std::cout<<"Direction Before Fixed 2D: "<<std::endl;
			   std::cout<<this->m_FixedImage->GetDirection()<<std::endl; 
			 */

			m_ResampleImageFilter = ResampleImageFilterType::New();

			m_ResampleImageFilter->SetTransform( this->m_Transform );
			m_ResampleImageFilter->SetInterpolator( this->m_Interpolator );
			m_ResampleImageFilter->SetInput( this->m_MovingImage );
			m_ResampleImageFilter->SetDefaultPixelValue(
					itk::NumericTraits<FixedImagePixelType>::Zero );

			//Copia de Datos provenientes de la Imagen Fija
			m_ResampleImageFilter->UseReferenceImageOn();
			m_ResampleImageFilter->SetReferenceImage( this->m_FixedImage );
			m_ResampleImageFilter->GetOutput()->SetRequestedRegion(this->GetFixedImageRegion() );


			m_ResampleImageFilter->Update();

			m_RescaleIntImageFilter = RescaleIntImageFilterType::New();
			m_RescaleIntImageFilter->SetOutputMinimum(   0 );
			m_RescaleIntImageFilter->SetOutputMaximum( 255 );

			//std::cout << "REGION" << this->m_FixedImage->GetBufferedRegion() << std::endl;

			m_RescaleIntImageFilter->SetInput( m_ResampleImageFilter->GetOutput());
			m_RescaleIntImageFilter->Update(); //Imagen movible proyeccion con  0-255 y threshold 0 por el interpolador

			//std::cout<<"Direction After 2D: "<<m_ResampleImageFilter->GetOutput()->GetDirection()<<std::endl;

			//Write the output images before to compute the NormalizedGradientCorrelation
			/*typename WriterType::Pointer movingfile = WriterType::New();
			  movingfile->SetFileName("moving2D.mha");
			  movingfile->SetInput(m_RescaleIntImageFilter->GetOutput());
			  movingfile->Update();				

			  typename WriterType::Pointer fixedfile = WriterType::New();
			  fixedfile->SetFileName("fixed2D.mha");
			  fixedfile->SetInput(this->m_FixedImage);
			  fixedfile->Update();			

			  std::cout<<"After Fixed 2D: "<<std::endl;
			  std::cout<<*this->m_FixedImage;

			  std::cout<<"After Moving 2D: "<<std::endl;
			  std::cout<<*m_RescaleIntImageFilter->GetOutput();
			 */
			//Para establecer correctamente las comparaciones la proyeccion de la imagen
			//movible sera rescalada en umbral de 0  a 255 y con esta recien ser comparada
			//con la respectiva imagen fija que tambien esta de 0 a 255



			for (unsigned int dim=0; dim < m_MaxDimension; dim++)
			{
				m_MovingSobelFilters[dim] = SobelFilterType::New();
				m_MovingSobelFilters[dim]->OverrideBoundaryCondition(
						&m_MovingBoundaryCondition );
				m_MovingSobelFilters[dim]->SetOperator( m_SobelOperators[dim] );
				m_MovingSobelFilters[dim]->SetInput(m_RescaleIntImageFilter->GetOutput());
				m_MovingSobelFilters[dim]->GetOutput()->SetRequestedRegion(
						this->GetFixedImageRegion() );
			}
		}


	/**
	 * PrintSelf
	 */
	template <class TFixedImage, class TMovingImage>
		void
		NormalizedGradientCorrelationImageToImageMetric<TFixedImage,TMovingImage>
		::PrintSelf(std::ostream& os, Indent indent) const
		{
			Superclass::PrintSelf( os, indent );
			os << indent << "DerivativeDelta: " << this->m_DerivativeDelta << std::endl;
		}


	/**
	 * Get the value of the similarity measure
	 */
	template <class TFixedImage, class TMovingImage>
		typename NormalizedGradientCorrelationImageToImageMetric<TFixedImage,TMovingImage>::MeasureType
		NormalizedGradientCorrelationImageToImageMetric<TFixedImage,TMovingImage>
		::GetValue( const TransformParametersType & parameters ) const
		{
			this->m_NumberOfPixelsCounted = 0;
			this->SetTransformParameters( parameters );			

			for (unsigned int dim=0; dim<m_MaxDimension; dim++)
			{	
				this->m_FixedSobelFilters[dim]->Update();

				// NGOC
				// Call Update() on this filter triggers Evaluate() method on RayCast interpolator
				// std::cout << " --- [GetValue] " << "Evaluate() called on dimension " << dim << std::endl;
				this->m_MovingSobelFilters[dim]->Update();
			}

			MeasureType val = NumericTraits< MeasureType >::Zero;

			/*
			 * cc: cross corrrelation
			 * fac: fixed auto correlation, this is, auto correlation of the fixed image
			 * mac: moving auto correlation, this is, moving image auto correlation
			 */
			MeasureType cc[FixedImageDimension];
			MeasureType fac[FixedImageDimension];
			MeasureType mac[FixedImageDimension];

			for( unsigned int dim=0; dim < m_MaxDimension; dim++ )
			{
				cc[dim] = NumericTraits< MeasureType >::Zero;
				fac[dim] = NumericTraits< MeasureType >::Zero;
				mac[dim] = NumericTraits< MeasureType >::Zero;
			}

			RealType movingGradient[FixedImageDimension];
			RealType fixedGradient[FixedImageDimension];

			FixedImageConstIteratorType iter( this->m_FixedImage,
					this->GetFixedImageRegion() );
			for( iter.GoToBegin(); !iter.IsAtEnd(); ++iter )
			{
				typename FixedImageType::IndexType fixedIndex = iter.GetIndex();

				//Check if point is inside the fixed image mask
				InputPointType inputPoint;
				this->GetFixedImage()->TransformIndexToPhysicalPoint( fixedIndex, inputPoint );

				if( this->m_FixedImageMask && !this->m_FixedImageMask->IsInsideInWorldSpace( inputPoint ) )
				{
					continue;
				}

				for( unsigned int dim=0; dim<m_MaxDimension; dim++ )
				{
					fixedGradient[dim] = m_FixedSobelFilters[dim]->GetOutput()->GetPixel(
							fixedIndex );
					movingGradient[dim] = m_MovingSobelFilters[dim]->GetOutput()->GetPixel(
							fixedIndex );
					cc[dim] +=  movingGradient[dim] * fixedGradient[dim];
					fac[dim] += fixedGradient[dim] * fixedGradient[dim];
					mac[dim] += movingGradient[dim] * movingGradient[dim];
				}

				this->m_NumberOfPixelsCounted++;
			}

			if( this->m_NumberOfPixelsCounted == 0 )
			{
				itkExceptionMacro(<< "No voxels counted for metric calculation");
			}

			for( unsigned int dim=0; dim < m_MaxDimension; dim++ )
			{
				if( fac[dim] == NumericTraits< MeasureType >::Zero ||
						mac[dim] == NumericTraits< MeasureType >::Zero )
				{
					itkExceptionMacro(<< "Auto correlation(s) equal to zero");
				}
			}

			for( unsigned int dim=0; dim < m_MaxDimension; dim++ )
			{
				val += cc[dim] / sqrt( fac[dim] * mac[dim] ) / m_MaxDimension;
			}

			return val;

		}
	template <class TFixedImage, class TMovingImage>
		void NormalizedGradientCorrelationImageToImageMetric<TFixedImage, TMovingImage>::writeSobelRes(std::string tipoImag){
			
			//verificar con que imagen estamos trabajando
			
			for (unsigned int dim=0; dim<2; dim++)
			{
				this->m_FixedSobelFilters[dim]->Update();
				this->m_MovingSobelFilters[dim]->Update();
			}


			typedef itk::Image<RealType,3> WriteImageType;
			typedef itk::ImageFileWriter<WriteImageType> WriterTypeSobel;
			typename WriterTypeSobel::Pointer wpoint;

			//Guardamos cada imagen de la salida del operador de vecindad
			//m_ResampleImageFilter = ResampleImageFilterType::New();
			wpoint = WriterTypeSobel::New();
			std::string namefile = "sobelfilter_fixed0"+tipoImag+".mha";
			wpoint->SetFileName(namefile);
			wpoint->SetInput(this->m_FixedSobelFilters[0]->GetOutput());
			wpoint->Update();
		
			namefile = "sobelfilter_fixed1"+tipoImag+".mha";	
			wpoint->SetFileName(namefile);
			wpoint->SetInput(this->m_FixedSobelFilters[1]->GetOutput());
			wpoint->Update();

			namefile = "sobelfilter_moving0"+tipoImag+".mha";
			wpoint->SetFileName(namefile);
			wpoint->SetInput(this->m_MovingSobelFilters[0]->GetOutput());
			wpoint->Update();
			
			namefile = "sobelfilter_moving1"+tipoImag+".mha";
			wpoint->SetFileName(namefile);
			wpoint->SetInput(this->m_MovingSobelFilters[1]->GetOutput());
			wpoint->Update();


			return;
		}
	//Write Moving Image by specific name file
		template <class TFixedImage, class TMovingImage>
		void NormalizedGradientCorrelationImageToImageMetric<TFixedImage, TMovingImage>::writeMovingImage(std::string namefile){
			
			typedef itk::ImageFileWriter<MovingImageType> WriterTypeSobel;
			typename WriterTypeSobel::Pointer wpoint;
			
			m_ResampleImageFilter->Update();
			m_RescaleIntImageFilter->Update(); 

			//Guardamos cada imagen de la salida del operador de vecindad
			//m_ResampleImageFilter = ResampleImageFilterType::New();
			wpoint = WriterTypeSobel::New();
			wpoint->SetFileName(namefile);
			wpoint->SetInput(m_RescaleIntImageFilter->GetOutput());
			wpoint->Update();


		}	
	/**
	 * Get the Derivative Measure
	 */
	template < class TFixedImage, class TMovingImage>
		void
		NormalizedGradientCorrelationImageToImageMetric<TFixedImage,TMovingImage>
		::GetDerivative( const TransformParametersType & parameters,
				DerivativeType & derivative           ) const
		{
			TransformParametersType testPoint;
			testPoint = parameters;

			const unsigned int numberOfParameters = this->GetNumberOfParameters();
			derivative = DerivativeType( numberOfParameters );

			for( unsigned int i=0; i<numberOfParameters; i++)
			{
				testPoint[i] -= this->m_DerivativeDelta;
				const MeasureType valuep0 = this->GetValue( testPoint );
				testPoint[i] += 2* this->m_DerivativeDelta;
				const MeasureType valuep1 = this->GetValue( testPoint );
				derivative[i] = (valuep1 - valuep0 ) / ( 2 * this->m_DerivativeDelta );
				testPoint[i] = parameters[i];
			}
		}


	/**
	 * Get both the match Measure and theDerivative Measure
	 */
	template <class TFixedImage, class TMovingImage>
		void
		NormalizedGradientCorrelationImageToImageMetric<TFixedImage,TMovingImage>
		::GetValueAndDerivative(const TransformParametersType & parameters,
				MeasureType & Value, DerivativeType  & Derivative) const
		{
			Value      = this->GetValue( parameters );
			this->GetDerivative( parameters, Derivative );
		}


	/**
	 * Set the parameters that define a unique transform
	 */
	template <class TFixedImage, class TMovingImage>
		void
		NormalizedGradientCorrelationImageToImageMetric<TFixedImage,TMovingImage>
		::SetTransformParameters( const TransformParametersType & parameters ) const
		{
			if( !this->m_Transform )
			{
				itkExceptionMacro(<<"Transform has not been assigned");
			}
			this->m_Transform->SetParameters( parameters );
		}

} // end namespace itk

#endif
