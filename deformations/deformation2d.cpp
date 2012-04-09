#include <sstream>
#include <iomanip>

/////////////////////
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

namespace po = boost::program_options;

/////////////////////
#include <DGtal/base/Common.h>
#include <DGtal/helpers/StdDefs.h>
#include "DGtal/io/readers/PNMReader.h"

using namespace Z2i;

//evolvers
//level set
#include "WeickertKuhneEvolver.h"

//phase field
#include "ExactDiffusionEvolver.h"
#include "ExplicitReactionEvolver.h"
#include "ExactReactionEvolver.h"
#include "LieSplittingEvolver.h"

//local level set
#include "SimplePointHelper.h"
#include "PartitionEvolver.h"

/////////////////////////// useful functions
#include "deformationFunctions.h"
#include "deformationDisplay2d.h"


///////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
  
  DGtal::trace.info() << "2d interface evolution using DGtal ";
  DGtal::trace.emphase() << "(version "<< DGTAL_VERSION << ")"<< std::endl; 

  // parse command line ----------------------------------------------
  po::options_description general_opt("Allowed options are");
  general_opt.add_options()
    ("help,h", "display this message")
    ("inputImage,i",  po::value<string>(), "Binary image to initialize the starting interface (.pgm)" )
    ("domainSize,d",  po::value<int>()->default_value(64), "Domain size (if default starting interface)" )
    ("shape,s", po::value<string>()->default_value("ball"), 
     "Generated shape: either <ball> (default) or <flower> " )
    ("timeStep,t",  po::value<double>()->default_value(0.25), "Time step for the evolution" )
    ("displayStep",  po::value<int>()->default_value(1), "Number of time steps between 2 drawings" )
    ("stepsNumber,n",  po::value<int>()->default_value(1), "Maximal number of steps" )
    ("algo,a",  po::value<string>()->default_value("levelSet"), 
     "can be: \n <levelSet>  \n or <phaseField> \n or <localLevelSet>" )
    ("balloonForce,k",  po::value<double>()->default_value(0.0), "Balloon force" )
    ("epsilon,e",  po::value<double>()->default_value(3.0), "Interface width (only for phase fields)" )
    ("withCstVol", "with volume conservation (only for phase fields)" )
    ("withFunction", po::value<string>(), "Output pgm file basename, where the starting implicit function is stored" )
    ("outputFiles,o", po::value<string>()->default_value("interface"), "Output files basename" )
    ("outputFormat,f", po::value<string>()->default_value("raster"), 
     "Output files format: either <raster> (image, default) or <vector> (domain representation)" );

  
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, general_opt), vm);  
  po::notify(vm);    
  if(vm.count("help")||argc<=1)
    {
      trace.info()<< "Evolution of a 2d interface" << std::endl
		  << "Basic usage: "<<std::endl
		  << argv[0] << " [other options] -t <time step> -n <number of steps>" << std::endl
		  << general_opt << "\n";
      return 0;
    }
  
  //Parse options
  //domain size
  int dsize; 
  if (!(vm.count("domainSize"))) trace.info() << "Domain size default value: 64" << std::endl; 
  dsize = vm["domainSize"].as<int>(); 

  //time step
  double tstep; 
  if (!(vm.count("timeStep"))) trace.info() << "time step default value: 1.0" << std::endl; 
  tstep = vm["timeStep"].as<double>(); 
    
  //iterations
  int step; 
  if (!(vm.count("displayStep"))) trace.info() << "number of steps between two drawings: 1 by default" << std::endl; 
  step = vm["displayStep"].as<int>(); 
  int max; 
  if (!(vm.count("stepsNumber"))) trace.info() << "maximal number of steps: 1 by default" << std::endl; 
  max = vm["stepsNumber"].as<int>(); 

  //files
  std::string outputFiles; 
  if (!(vm.count("outputFiles"))) 
    trace.info() << "output files beginning with : interface" << std::endl;
  outputFiles = vm["outputFiles"].as<std::string>();

  //files format
  std::string format; 
  if (!(vm.count("outputFormat"))) 
    trace.info() << "output files format is 'vector' " << std::endl;
  format = vm["outputFormat"].as<std::string>();
  if ((format != "vector")&&(format != "raster")) 
    {
      trace.info() << "format is expected to be either vector, or raster " << std::endl;
      return 0; 
    }

  //image and implicit function
  typedef ImageContainerBySTLVector<Domain,short int> LabelImage;
  LabelImage* labelImage = NULL; 
 
  if (vm.count("inputImage")) 
    { 
      string imageFileName = vm["inputImage"].as<std::string>();
      trace.emphase() << imageFileName <<std::endl; 
      DGtal::trace.beginBlock("image reading..."); 
      LabelImage tmp = PNMReader<LabelImage>::importPGM( imageFileName ); 
      labelImage = new LabelImage( tmp ); 
      DGtal::trace.endBlock(); 
    }
  else
    {
      DGtal::trace.beginBlock("image reading..."); 
      Point p(0,0);
      Point q(dsize,dsize); 
      Point c(dsize/2,dsize/2); 
      labelImage = new LabelImage( Domain(p,q) ); 
      if (vm.count("shape"))
        {
          if ( (vm["shape"].as<std::string>()) == "flower" )
	    initWithFlowerPredicate( *labelImage, c, (dsize*3/5)/2, (dsize*1/5)/2, 5 );
          else 
	    initWithBallPredicate( *labelImage, c, (dsize*3/5)/2 );
        } 
      else 
        initWithBallPredicate( *labelImage, c, (dsize*3/5)/2 );
      trace.info() << "starting interface initialized with a " 
		   << (vm["shape"].as<std::string>()) << std::endl;
      DGtal::trace.endBlock(); 
    }

  //domain
  Domain d = Domain( labelImage->domain().lowerBound(), labelImage->domain().upperBound() );

  
  //algo
  std::string algo; 
  if (!(vm.count("algo"))) trace.info() << "default algorithm: levelSet" << std::endl; 
  algo = vm["algo"].as<string>(); 

  if (algo.compare("levelSet")==0)
    {

      //balloon force
      double k; 
      if (!(vm.count("balloonForce"))) trace.info() << "balloon force default value: 0" << std::endl; 
      k = vm["balloonForce"].as<double>(); 

      ImageContainerBySTLVector<Domain,double> implicitFunction( d ); 
      initWithDT( *labelImage, implicitFunction );

      if (vm.count("withFunction")) 
	drawFunction( implicitFunction, vm["withFunction"].as<string>() ); 

      std::stringstream ss; 
      ss << outputFiles << "0001"; 
      drawContour(implicitFunction, ss.str(), format); 

      //data functions
      ImageContainerBySTLVector<Domain,double> a( d ); 
      std::fill(a.begin(),a.end(), 1.0 );  
      ImageContainerBySTLVector<Domain,double> b( d ); 
      std::fill(b.begin(),b.end(), 1.0 );  
      ImageContainerBySTLVector<Domain,double> g( d ); 
      std::fill(g.begin(),g.end(), 1.0 );  

      //evolution
      WeickertKuhneEvolver<ImageContainerBySTLVector<Domain,double> > e(a,b,g,k,1); 

      DGtal::trace.beginBlock( "Deformation (Weickert level set method)" );

      for (unsigned int i = 1; i <= max; ++i) 
	{

	  DGtal::trace.info() << "iteration # " << i << std::endl;

	  e.update( implicitFunction, tstep); 

	  if ((i%step)==0) 
	    {
	      std::stringstream s; 
	      s << outputFiles << setfill('0') << std::setw(4) << (i/step)+1; 
	      drawContour(implicitFunction, s.str(), format); 
	    }

	  //area
	  trace.info() << "# area: " << getSize( implicitFunction ) << std::endl; 
	}

      DGtal::trace.endBlock(); 

    } else if (algo.compare("phaseField")==0)
    {

      double epsilon = 3.0; 
      if (!(vm.count("epsilon"))) trace.info() << "epsilon default value: 3.0" << std::endl; 
      epsilon = vm["epsilon"].as<double>(); 
      if (epsilon <= 0) 
	{
	  trace.error() << "epsilon should be greater than 0" << std::endl;
	  return 0; 
	} 

      bool flagWithCstVol = false; 
      if (vm.count("withCstVol")) 
	flagWithCstVol = true; 

      ImageContainerBySTLVector<Domain,double> implicitFunction( d ); 
      initWithDT( *labelImage, implicitFunction );

      //computing the profile from the signed distance
      Profile p(epsilon); 
      std::transform(implicitFunction.begin(), implicitFunction.end(), implicitFunction.begin(), p); 

      if (vm.count("withFunction")) 
        drawFunction( implicitFunction, vm["withFunction"].as<string>() ); 

      std::stringstream ss; 
      ss << outputFiles << "0001"; 
      drawContour(implicitFunction, ss.str(), format, 0.5); 



      typedef ExactDiffusionEvolver<ImageContainerBySTLVector<Domain,double> > Diffusion; 
      typedef ExactReactionEvolver<ImageContainerBySTLVector<Domain,double> > Reaction; 
      Diffusion diffusion; 
      Reaction reaction( epsilon );
      // typedef ExplicitReactionEvolver<ImageContainerBySTLVector<Domain,double>, 
      //   ImageContainerBySTLVector<Domain,double> > Reaction; 
      // ImageContainerBySTLVector<Domain,double> a( implicitFunction.domain() ); 
      // std::fill(a.begin(),a.end(), 1 );  
      // Diffusion diffusion; 
      // Reaction reaction( epsilon, a, k, flagWithCstVol );
      LieSplittingEvolver<Diffusion,Reaction> e(diffusion, reaction); 

      DGtal::trace.beginBlock( "Deformation (phase field)" );

      for (unsigned int i = step; i <= max; i += step) 
	{

	  DGtal::trace.info() << "iteration # " << i << std::endl;

	  e.update( implicitFunction, tstep); 

	  if ((i%step)==0) 
	    {
	      std::stringstream s; 
	      s << outputFiles << setfill('0') << std::setw(4) << (i/step)+1; 
	      drawContour(implicitFunction, s.str(), format, 0.5); 
	    }

	  //area
	  trace.info() << "# area: " << getSize( implicitFunction, 0.5 ) << std::endl; 

	}

      DGtal::trace.endBlock();
    } else if (algo.compare("localLevelSet")==0)
    {

      std::stringstream ss; 
      ss << outputFiles << "0001"; 
      drawContours(*labelImage, ss.str(), format);  

      //space
      KSpace ks;
      ks.init( d.lowerBound(), d.upperBound(), true ); 
   
      //distance image...
      typedef ImageContainerBySTLVector<Domain,double> DistanceImage; 
      //and extern data...
      DistanceImage g( d );
      std::fill(g.begin(), g.end(), 1.0); 

      // topological predicate
      typedef SimplePointHelper<LabelImage> TopologicalPredicate; 
      TopologicalPredicate topologicalPredicate(*labelImage); 
      
      //frontier evolver
      DGtal::trace.beginBlock( "Partition construction" );
      PartitionEvolver<KSpace, LabelImage, DistanceImage, DistanceImage, 
	TopologicalPredicate> 
	e(ks, *labelImage, g, topologicalPredicate); 

      DGtal::trace.info() << e << std::endl; 
      DGtal::trace.endBlock(); 
      
      DGtal::trace.beginBlock( "Deformation (narrow band with topological control)" );

      double sumt = 0; 
      for (unsigned int i = 1; i <= max; ++i) 
	{
	  DGtal::trace.info() << "iteration # " << i << std::endl; 

	  //update
	  e.update(tstep); 

	  if ((i%step)==0) 
	    {
	      //display
	      std::stringstream s; 
	      s << outputFiles << setfill('0') << std::setw(4) << (i/step)+1; 
	      drawContours(*labelImage, s.str(), format); 
	    }
	  sumt += tstep; 
	}

      DGtal::trace.endBlock();      

    } else trace.error() << "unknown algo. Try option -h to see the available algorithms " << std::endl;

  //free
  delete( labelImage ); 
  
  return 1;
}

