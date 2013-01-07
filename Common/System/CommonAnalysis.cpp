/********************************************************************************
    CommonAnalysis.cpp
    NairnFEA and NairnMPM
    
    Created by John Nairn on Jan 13, 2006.
    Copyright (c) 2006 John A. Nairn, All rights reserved.
********************************************************************************/

#ifdef MPM_CODE
	#include "System/ArchiveData.hpp"
	#include "Read_MPM/MPMReadHandler.hpp"
#else
	#include "System/FEAArchiveData.hpp"
	#include "Read_FEA/FEAReadHandler.hpp"
#endif
#include "Exceptions/CommonException.hpp"
#include "Exceptions/StrX.hpp"
#include "Materials/MaterialBase.hpp"
#include "Nodes/NodalPoint.hpp"
#include "Elements/ElementBase.hpp"

// reeading globals
static bool doSchema=TRUE;
static bool schemaFullChecking=FALSE;

/********************************************************************************
	CommonAnalysis: Constructors and Destructor
********************************************************************************/

CommonAnalysis::CommonAnalysis()
{
	validate=FALSE;				// validate with DTD file
	reverseBytes=FALSE;			// reverse bytes in archived files
	description=NULL;
	SetDescription("\nNo Description");
	
	np=-1;						// analysis method to be set
	nfree=2;					// 2D analysis
	
	// current uses
	// dflag[0] - affects normal calcualtion in contact
	int i;
	for(i=0;i<NUMBER_DEVELOPMENT_FLAGS;i++) dflag[i]=0;
}

CommonAnalysis::~CommonAnalysis()
{
	delete [] description;
}

/********************************************************************************
	CommonAnalysis: methods
********************************************************************************/

// start results file before proceeding to analysis
void CommonAnalysis::StartResultsOutput(void)
{
    //--------------------------------------------------
    // Analysis Title
	PrintAnalysisTitle();
	cout << "Written by: Nairn Research Group, Oregon State University\n"
         << "Date: " << __DATE__ << "\n\n";
         
    //--------------------------------------------------
    // Description
    PrintSection("ANALYSIS DESCRIPTION");
    cout << GetDescription() << endl;
	
	/* development flags
	 *
	 *	Current Flags in Use in MPM Code
	 *   [0]==1 - use MPM standard contact where each material finds normal from its own gradietn
	 *   [0]==2 - find normal by averaging the volume gradient of the two contacting materials
	 *   [0]==4 - cutting simulations - find normal for tool angle and the angle (interger only)
	 *				is in flag [1]
	 *   [1]==rake angle - cutting simulations when [0]==4
	 */
	int i;
	bool hasFlags=FALSE;
	for(i=0;i<NUMBER_DEVELOPMENT_FLAGS;i++)
	{	if(dflag[i]!=0)
		{	cout << "Development Flag #" << i << " = " << dflag[i] << endl;
			hasFlags=TRUE;
		}
	}
	if(hasFlags) cout << endl;
	
    //--------------------------------------------------
    // Analysis Type
	PrintAnalysisType();
	
    // start section (Background Grid)
#ifdef MPM_CODE
    PrintSection("NODES AND ELEMENTS (Background Grid)");
#else
    PrintSection("NODES AND ELEMENTS");
#endif
    
    //---------------------------------------------------
    // Nodes
	char hline[200];
    if(nnodes==0 || nelems<0)
        throw CommonException("No nodes or elements were defined in the input file.","CommonAnalysis::StartResultsOutput");
    sprintf(hline,"Nodes: %d       Elements: %d\n",nnodes,nelems);
    cout << hline;
    sprintf(hline,"DOF per node: %d     ",nfree);
    cout << hline;
    
	// analysis type
    switch(np)
    {	case PLANE_STRAIN:
            cout << "2D Plane Strain Analysis\n";
            break;
        
        case PLANE_STRESS:
            cout << "2D Plane Stress Analysis\n";
            break;
        
        case AXI_SYM:
            cout << "Axisymmetric Analysis\n";
            break;
        
    	case PLANE_STRAIN_MPM:
            cout << "2D Plane Strain MPM Analysis\n";
            break;
        
        case PLANE_STRESS_MPM:
            cout << "2D Plane Stress MPM Analysis\n";
            break;
        
        case THREED_MPM:
            cout << "3D MPM Analysis\n";
            break;
		
		case AXISYMMETRIC_MPM:
            cout << "Axisymmetric MPM Analysis\n";
            break;
        
        default:
            throw CommonException("No FEA or MPM analysis type was provided.","CommonAnalysis::StartResultsOutput");
    }
#ifdef MPM_CODE
	cout << "Incremental F Terms: " << MaterialBase::incrementalDefGradTerms << endl;
#endif
	cout << endl;
	
    //---------------------------------------------------
    // Nodes
    PrintSection("NODAL POINT COORDINATES (in mm)");
	archiver->ArchiveNodalPoints(np);

    //---------------------------------------------------
    // Elements
    PrintSection("ELEMENT DEFINITIONS");
	archiver->ArchiveElements(np);
    
    //---------------------------------------------------
    // Defined Materials
	const char *err;
#ifdef MPM_CODE
    PrintSection("DEFINED MATERIALS\n       (Note: moduli and stresses in MPa, thermal exp. coeffs in ppm/C)\n       (      density in g/cm^3)");
#else
    PrintSection("DEFINED MATERIALS\n       (Note: moduli in MPa, thermal exp. coeffs in ppm/C)");
#endif
    if(theMaterials==NULL)
        throw CommonException("No materials were defined.","CommonAnalysis::StartResultsOutput");
    for(i=0;i<nmat;i++)
    {	err=theMaterials[i]->VerifyProperties(np);
    	theMaterials[i]->PrintMaterial(i+1);
		cout << endl;
        if(err!=NULL)
		{	cout << "Invalid material properties\n   " << err << endl;
            throw CommonException("See material error above.","CommonAnalysis::StartResultsOutput");
		}
   }
	
	// finish with analysis specific items
	MyStartResultsOutput();
}

//Main entry to read file and decode into objects
int CommonAnalysis::ReadFile(const char *xmlFile,bool useWorkingDir)
{
	// set directory of input file
	archiver->SetInputDirPath(xmlFile,useWorkingDir);
	
    // Initialize the XML4C2 system
	SAX2XMLReader* parser;
    try
    {	XMLPlatformUtils::Initialize();
	
		//  Create a SAX parser object.
		parser=XMLReaderFactory::createXMLReader();
		
		// Validation (1st means yes or no, 2nd means to skip if no DTD, even if yes)
		parser->setFeature(XMLString::transcode("http://xml.org/sax/features/validation"),GetValidate());
		parser->setFeature(XMLString::transcode("http://apache.org/xml/features/validation/dynamic"),true);
		if(!GetValidate())
		{	parser->setFeature(XMLString::transcode("http://apache.org/xml/features/nonvalidating/load-external-dtd"),
									GetValidate());
		}

		// default settings
		parser->setFeature(XMLString::transcode("http://apache.org/xml/features/validation/schema"),doSchema);
		parser->setFeature(XMLString::transcode("http://apache.org/xml/features/validation/schema-full-checking"),schemaFullChecking);
    }

    catch(const XMLException& toCatch)
    {   cerr << "\nAn error occured while initializing Xerces: "
            << StrX(toCatch.getMessage()) << endl;
        return XercesInitErr;
    }
	
    catch( ... )
    {	cerr << "\nUnexpected error initializing Xerces" << endl;
		return XercesInitErr;
    }

    // parce the file
#ifdef MPM_CODE
	MPMReadHandler *handler=new MPMReadHandler();
#else
	FEAReadHandler *handler=new FEAReadHandler();
#endif
    try
	{	// create the handler
		parser->setContentHandler(handler);
		parser->setErrorHandler(handler);
		parser->parse(xmlFile);
		
		// Reading done, not clean up and exit
#ifdef MPM_CODE
		delete parser;				// Must be done prior to calling Terminate, below.
#else
		// delete parser;			// not sure why cannot delete the parser in FEA code
#endif
		delete handler;
		XMLPlatformUtils::Terminate();
    }

    catch(const XMLException& toCatch)
	{	char *message = XMLString::transcode(toCatch.getMessage());
        cerr << "\nParcing error: " << message << endl;
		//XMLString::release(&message); // commented out because inferno (and maybe others) can't take it
        return ReadFileErr;
    }
    
    catch(const SAXException& err)
	{	char *message = XMLString::transcode(err.getMessage());
    	cerr << "\nInput file error: " << message << endl;
		//XMLString::release(&message); // commented out because inferno (and maybe others) can't take it
        return ReadFileErr;
    }
	
	catch(const char *errMsg)
	{	cerr << errMsg << endl;
		return ReadFileErr;
	}
    
    catch( ... )
    {	cerr << "Unexpected error occurred while reading XML file" << endl;
		return ReadFileErr;
    }
    
    return noErr;
}

// print conde version to standard output
void CommonAnalysis::CoutCodeVersion(void)
{
	char *name=CodeName();
	cout << name << " " << version << "." << subversion << " build " << buildnumber << endl;
	delete [] name;
}

/********************************************************************************
	CommonAnalysis: accessors
********************************************************************************/

// validation option
void CommonAnalysis::SetValidate(bool setting) { validate=setting; }
bool CommonAnalysis::GetValidate(void) { return validate; }

// reverse bytes (MPM only)
void CommonAnalysis::SetReverseBytes(bool setting) { reverseBytes=setting; }
bool CommonAnalysis::GetReverseBytes(void) { return reverseBytes; }

// description
void CommonAnalysis::SetDescription(const char *descrip)
{	if(description!=NULL) delete [] description;
    description=new char[strlen(descrip)+1];
    strcpy(description,descrip);
}
// point to second character because first is always new line character
char *CommonAnalysis::GetDescription(void) { return &description[1]; }

// is it 3D analysis
bool CommonAnalysis::IsThreeD(void) { return np==THREED_MPM; }
bool CommonAnalysis::IsThreeD(int otherNp) { return otherNp==THREED_MPM; }

// is it axisynmmetric (FEA or MPM)
bool CommonAnalysis::IsAxisymmetric(void) { return np==AXI_SYM || np==AXISYMMETRIC_MPM; }
bool CommonAnalysis::IsAxisymmetric(int otherNp) { return otherNp==AXI_SYM || otherNp==AXISYMMETRIC_MPM; }

/********************************************************************************
	Methods to keep Xerces contact in fewer files
********************************************************************************/

// throw an SAXException
void ThrowSAXException(const char *msg)
{	throw SAXException(msg);
}

// throw an SAXException with format string and one string variable
void ThrowSAXException(const char *frmt,const char *msg)
{	char eline[500];
	sprintf(eline,frmt,msg);
	throw SAXException(eline);
}



