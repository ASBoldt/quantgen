/** \file eqtlbma.cpp
 *
 *  `eqtlbma' performs eQTL mapping via a Bayesian meta-analysis model.
 *  Copyright (C) 2012 Timothee Flutre
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  g++ -Wall -DEQTLBMA_MAIN gzstream/gzstream.C utils.cpp eqtlbma.cpp -lgsl -lgslcblas -lz -o eqtlbma
 */

#include <cmath>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <getopt.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <limits>
#include <sstream>
#include <numeric>
using namespace std;

#include <gsl/gsl_sort.h>
#include <gsl/gsl_sort_vector.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_combination.h>

#include "utils.h"

/** \brief Display the help on stdout.
 */
void help (char ** argv)
{
  cout << "`" << argv[0] << "'"
       << " performs eQTL mapping via a Bayesian meta-analysis model." << endl
       << endl
       << "Usage: " << argv[0] << " [OPTIONS] ..." << endl
       << endl
       << "Options:" << endl
       << "  -h, --help\tdisplay the help and exit" << endl
       << "  -V, --version\toutput version information and exit" << endl
       << "  -v, --verbose\tverbosity level (0/default=1/2/3)" << endl
       << "  -g, --geno\tfile with absolute paths to genotype files" << endl
       << "\t\ttwo columns: subgroup identifier<space/tab>path to file" << endl
       << "\t\tcan be a single line (eg. for multiple tissues) but identifier of first subgroup" << endl
       << "\t\teach file should be in IMPUTE format (delimiter: space or tab)" << endl
       << "\t\ta header line with sample names is required" << endl
       << "\t\tadd '#' at the beginning of a line to comment it" << endl
       << "  -p, --pheno\tfile with absolute paths to phenotype files" << endl
       << "\t\ttwo columns: subgroup identifier<space/tab>path to file" << endl
       << "\t\tcan be a single line (single subgroup)" << endl
       << "\t\trow 1 for sample names, column 1 for feature names" << endl
       << "\t\tsubgroups can have different features" << endl
       << "\t\tall features should be in the --fcoord file" << endl
       << "\t\tadd '#' at the beginning of a line to comment it" << endl
       << "      --fcoord\tfile with the features coordinates" << endl
       << "\t\tshould be in the BED format (delimiter: tab)" << endl
       << "      --anchor\tfeature boundary(ies) for the cis region" << endl
       << "\t\tdefault=FSS, can also be FSS+FES" << endl
       << "      --cis\tlength of half of the cis region (in bp)" << endl
       << "\t\tapart from the anchor(s), default=100000" << endl
       << "  -o, --out\tprefix for the output files" << endl
       << "\t\tall output files are gzipped" << endl
       << "      --step\tstep of the analysis to perform" << endl
       << "\t\t1: only separate analysis of each subgroup, without permutation" << endl
       << "\t\t2: only separate analysis of each subgroup, with permutation" << endl
       << "\t\t3: both separate and joint analysis, without permutation" << endl
       << "\t\t4: both separate and joint analysis, with permutation for joint only" << endl
       << "\t\t5: both separate and joint analysis, with permutation for both" << endl
       << "      --qnorm\tquantile-normalize the phenotypes" << endl
       << "      --grid\tfile with the grid of values for phi2 and omega2 (ES model)" << endl
       << "\t\tsee GetGridPhiOmega() in package Rquantgen" << endl
       << "      --bfs\twhich Bayes Factors to compute for the joint analysis" << endl
       << "\t\tdefault='const': for the consistent configuration (+fixed-effect)" << endl
       << "\t\t'subset': compute also the BFs for each subgroup-specific configurations" << endl
       << "\t\t'all': compute also the BFs for all configurations" << endl
       << "      --nperm\tnumber of permutations" << endl
       << "\t\tdefault=0, recommended=10000" << endl
       << "      --seed\tseed for the two random number generators" << endl
       << "\t\tone for the permutations, another for the trick" << endl
       << "\t\tby default, both are initialized via microseconds from epoch" << endl
       << "\t\tthe RNGs are re-seeded before each subgroup and before the joint analysis" << endl
       << "      --trick\tapply trick to speed-up permutations" << endl
       << "\t\tstop after the tenth permutation for which the test statistic" << endl
       << "\t\tis better than or equal to the true value, and sample from" << endl
       << "\t\ta uniform between 11/(nbPermsSoFar+2) and 11/(nbPermsSoFar+1)" << endl
       << "\t\tif '1', the permutations really stops" << endl
       << "\t\tif '2', all permutations are done but the test statistics are not computed" << endl
       << "\t\tallow to compare different test statistics on the same permutations" << endl
       << "      --pbf\twhich BF to use as the test statistic for the joint-analysis permutations" << endl
       << "\t\tdefault=const/subset/all" << endl
       << "  -f, --ftr\tfile with a list of features to analyze" << endl
       << "\t\tone feature name per line" << endl
       << "  -s, --snp\tfile with a list of SNPs to analyze" << endl
       << "\t\tone SNP name per line" << endl
       << endl;
}

/** \brief Display version and license information on stdout.
 */
void version (char ** argv)
{
  cout << argv[0] << " 0.1" << endl
       << endl
       << "Copyright (C) 2012 T. Flutre." << endl
       << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>" << endl
       << "This is free software; see the source for copying conditions.  There is NO" << endl
       << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE." << endl
       << endl
       << "Written by T. Flutre." << endl;
}

/** \brief Parse the command-line arguments and check the values of the 
 *  compulsory ones.
 */
void
parseArgs (
  int argc,
  char ** argv,
  string & genoPathsFile,
  string & phenoPathsFile,
  string & ftrCoordsFile,
  string & anchor,
  size_t & lenCis,
  string & outPrefix,
  int & whichStep,
  bool & needQnorm,
  string & gridFile,
  string & whichBfs,
  size_t & nbPerms,
  size_t & seed,
  int & trick,
  string & whichPermBf,
  string & ftrsToKeepFile,
  string & snpsToKeepFile,
  int & verbose)
{
  int c = 0;
  while (1)
  {
    static struct option long_options[] =
      {
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"verbose", required_argument, 0, 'v'},
	{"geno", required_argument, 0, 'g'},
	{"pheno", required_argument, 0, 'p'},
	{"fcoord", required_argument, 0, 0},
	{"anchor", required_argument, 0, 0},
	{"cis", required_argument, 0, 0},
	{"out", required_argument, 0, 'o'},
	{"step", required_argument, 0, 0},
	{"qnorm", no_argument, 0, 0},
	{"grid", required_argument, 0, 0},
	{"bfs", required_argument, 0, 0},
	{"nperm", required_argument, 0, 0},
	{"seed", required_argument, 0, 0},
	{"trick", required_argument, 0, 0},
	{"pbf", required_argument, 0, 0},
	{"ftr", required_argument, 0, 'f'},
	{"snp", required_argument, 0, 's'},
	{0, 0, 0, 0}
      };
    int option_index = 0;
    c = getopt_long (argc, argv, "hVv:g:p:o:f:s:",
		     long_options, &option_index);
    if (c == -1)
      break;
    switch (c)
    {
    case 0:
      if (long_options[option_index].flag != 0)
	break;
      if (strcmp(long_options[option_index].name, "fcoord") == 0)
      {
	ftrCoordsFile = optarg;
	break;
      }
      if (strcmp(long_options[option_index].name, "anchor") == 0)
      {
	anchor = optarg;
	break;
      }
      if (strcmp(long_options[option_index].name, "cis") == 0)
      {
	lenCis = atol (optarg);
	break;
      }
      if (strcmp(long_options[option_index].name, "step") == 0)
      {
	whichStep = atoi (optarg);
	break;
      }
      if (strcmp(long_options[option_index].name, "qnorm") == 0)
      {
	needQnorm = true;
	break;
      }
      if (strcmp(long_options[option_index].name, "grid") == 0)
      {
	gridFile = optarg;
	break;
      }
      if (strcmp(long_options[option_index].name, "bfs") == 0)
      {
	whichBfs = optarg;
	break;
      }
      if (strcmp(long_options[option_index].name, "nperm") == 0)
      {
	nbPerms = atol (optarg);
	break;
      }
      if (strcmp(long_options[option_index].name, "seed") == 0)
      {
	seed = atol (optarg);
	break;
      }
      if (strcmp(long_options[option_index].name, "trick") == 0)
      {
	trick = atoi (optarg);
	break;
      }
      if (strcmp(long_options[option_index].name, "pbf") == 0)
      {
	whichPermBf = optarg;
	break;
      }
    case 'h':
      help (argv);
      exit (0);
    case 'V':
      version (argv);
      exit (0);
    case 'v':
      verbose = atoi (optarg);
      break;
    case 'g':
      genoPathsFile = optarg;
      break;
    case 'p':
      phenoPathsFile = optarg;
      break;
    case 'o':
      outPrefix = optarg;
      break;
    case 'f':
      ftrsToKeepFile = optarg;
      break;
    case 's':
      snpsToKeepFile = optarg;
      break;
    case '?':
      printf ("\n"); help (argv);
      abort ();
    default:
      printf ("\n"); help (argv);
      abort ();
    }
  }
  if (genoPathsFile.empty())
  {
    fprintf (stderr, "ERROR: missing compulsory option -g\n\n");
    help (argv);
    exit (1);
  }
  if (! doesFileExist (genoPathsFile))
  {
    fprintf (stderr, "ERROR: can't file '%s'\n\n", genoPathsFile.c_str());
    help (argv);
    exit (1);
  }
  if (phenoPathsFile.empty())
  {
    fprintf (stderr, "ERROR: missing compulsory option -p\n\n");
    help (argv);
    exit (1);
  }
  if (! doesFileExist (phenoPathsFile))
  {
    fprintf (stderr, "ERROR: can't find '%s'\n\n", phenoPathsFile.c_str());
    help (argv);
    exit (1);
  }
  if (ftrCoordsFile.empty())
  {
    fprintf (stderr, "ERROR: missing compulsory option --fcoord\n\n");
    help (argv);
    exit (1);
  }
  if (! doesFileExist (ftrCoordsFile))
  {
    fprintf (stderr, "ERROR: can't find '%s'\n\n", ftrCoordsFile.c_str());
    help (argv);
    exit (1);
  }
  if (anchor.empty())
  {
    fprintf (stderr, "ERROR: SNPs in trans not yet implemented, see --anchor and --cis\n\n");
    help (argv);
    exit (1);
  }
  if (outPrefix.empty())
  {
    fprintf (stderr, "ERROR: missing compulsory option -o\n\n");
    help (argv);
    exit (1);
  }
  if (whichStep != 1 && whichStep != 2 && whichStep != 3 && whichStep != 4
      && whichStep != 5)
  {
    fprintf (stderr, "ERROR: --step should be 1, 2, 3, 4 or 5\n\n");
    help (argv);
    exit (1);
  }
  if ((whichStep == 3 || whichStep == 4 || whichStep == 5) && gridFile.empty())
  {
    fprintf (stderr, "ERROR: missing compulsory options --grid when --step is 3, 4 or 5\n\n");
    help (argv);
    exit (1);
  }
  if (! gridFile.empty() && ! doesFileExist (gridFile))
  {
    fprintf (stderr, "ERROR: can't find '%s'\n\n", gridFile.c_str());
    help (argv);
    exit (1);
  }
  if (whichBfs.compare("const") != 0 && whichBfs.compare("subset") != 0
      && whichBfs.compare("all") != 0)
  {
    fprintf (stderr, "ERROR: --bf should be 'const', 'subset' or 'all'\n\n");
    help (argv);
    exit (1);
  }
  if ((whichStep == 2 || whichStep == 4 || whichStep == 5) && nbPerms == 0)
  {
    fprintf (stderr, "ERROR: --step %i but nbPerms = 0, see --nperm\n\n", whichStep);
    help (argv);
    exit (1);
  }
  if (trick != 0 && trick != 1 && trick != 2)
  {
    fprintf (stderr, "ERROR: --trick should be 0, 1 or 2\n\n");
    help (argv);
    exit (1);
  }
  if ((whichStep == 4 || whichStep == 5) && whichBfs.compare("const") == 0
      && whichPermBf.compare("const") != 0)
  {
    fprintf (stderr, "ERROR: if --bfs const, then --pbf should be const\n\n");
    help (argv);
    exit (1);
  }
  if ((whichStep == 4 || whichStep == 5) && whichBfs.compare("subset") == 0
      && whichPermBf.compare("all") == 0)
  {
    fprintf (stderr, "ERROR: if --bfs subset, then --pbf should be const or subset\n\n");
    help (argv);
    exit (1);
  }
  if (! ftrsToKeepFile.empty() && ! doesFileExist (ftrsToKeepFile))
  {
    fprintf (stderr, "ERROR: can't find '%s'\n\n", ftrsToKeepFile.c_str());
    help (argv);
    exit (1);
  }
  if (! snpsToKeepFile.empty() && ! doesFileExist (snpsToKeepFile))
  {
    fprintf (stderr, "ERROR: can't find '%s'\n\n", snpsToKeepFile.c_str());
    help (argv);
    exit (1);
  }
  if (seed == string::npos)
    seed = getSeed ();
}

vector< vector<double> >
loadGrid (
  const string & gridFile,
  const int & verbose)
{
  vector< vector<double> > grid;
  
  if (gridFile.empty())
    return grid;
  
  if (verbose > 0)
    cout << "load grid ..." << endl << flush;
  
  ifstream gridStream;
  vector<string> tokens;
  string line;
  openFile (gridFile, gridStream);
  while (gridStream.good())
  {
    getline (gridStream, line);
    if (line.empty())
      break;
    split (line, " \t", tokens);
    if (tokens.size() != 2)
    {
      cerr << "ERROR: format of file " << gridFile
	   << " should be phi2<space/tab>oma2" << endl;
      exit (1);
    }
    vector<double> grid_values;
    grid_values.push_back (atof (tokens[0].c_str()));
    grid_values.push_back (atof (tokens[1].c_str()));
    grid.push_back (grid_values);
  }
  gridStream.close();
  
  if (verbose > 0)
    cout << "grid size: " << grid.size() << endl;
  
  return grid;
}

/** \brief Compute the summary statistics of the simple linear regression.
 *  \note y_i = mu + g_i * beta + e_i with e_i ~ N(0,sigma^2)
 *  \note missing values should have been already filtered out
 */
void
ols (
  const vector<double> & g,
  const vector<double> & y,
  double & betahat,
  double & sebetahat,
  double & sigmahat,
  double & pval,
  double & R2)
{
  size_t i = 0, n = g.size();
  double ym = 0, gm = 0, yty = 0, gtg = 0, gty = 0;
  for(i=0; i<n; ++i){
    ym += y[i];
    gm += g[i];
    yty += y[i] * y[i];
    gtg += g[i] * g[i];
    gty += g[i] * y[i];
  }
  ym /= n;
  gm /= n;
  double vg = gtg - n * gm * gm;  // variance of g
#ifdef DEBUG
  printf ("n=%zu ym=%f gm=%f yty=%f gtg=%f gty=%f vg=%f\n",
	  n, ym, gm, yty, gtg, gty, vg);
#endif
  if(vg > 1e-8)
  {
    betahat = (gty - n * gm * ym) / vg;
#ifdef DEBUG
    printf ("betahat=%f\n", betahat);
#endif
    double rss1 = yty - 1/vg * (n*ym*(gtg*ym - gm*gty) - gty*(n*gm*ym - gty));
    if (fabs(betahat) > 1e-8)
      sigmahat = sqrt(rss1 / (n-2));
    else  // case where y is not variable enough among samples
      sigmahat = sqrt((yty - n * ym * ym) / (n-2));  // sqrt(rss0/(n-2))
#ifdef DEBUG
    printf ("sigmahat=%f\n", sigmahat);
#endif
    sebetahat = sigmahat / sqrt(gtg - n*gm*gm);
#ifdef DEBUG
    printf ("sebetahat=%f\n", sebetahat);
#endif
    double muhat = (ym*gtg - gm*gty) / (gtg - n*gm*gm);
    double mss = 0;
    for(i=0; i<n; ++i)
      mss += pow(muhat + betahat * g[i] - ym, 2);
    pval = gsl_cdf_fdist_Q (mss/pow(sigmahat,2), 1, n-2);
    R2 = mss / (mss + rss1);
  }
  else
  {
#ifdef DEBUG
    cout << "g is not variable enough among samples" << endl << flush;
#endif
    betahat = 0;
    sebetahat = numeric_limits<double>::infinity();
    sigmahat = sqrt((yty - n * ym * ym) / (n-2));  // sqrt(rss0/(n-2))
    pval = 1;
    R2 = 0;
  }
}

struct Snp
{
  string name; // eg. rs7263524
  string chr; // eg. chr21
  size_t coord; // 1-based coordinate
  vector<vector<double> > vvGenos; // genotypes of samples per subgroup
  vector<vector<bool> > vvIsNa; // missing values per subgroup
  vector<double> vMafs; // minor allele frequencies per subgroup
};

struct ResFtrSnp
{
  string snp; // name of the SNP
  vector<size_t> vNs; // sample sizes per subgroup
  vector<double> vBetahats; // MLE of the beta per subgroup
  vector<double> vSebetahats; // standard errors
  vector<double> vSigmahats; // MLEs of the sigmas
  vector<double> vBetaPvals; // P-values of H0:"beta=0" and H1:"beta!=0"
  vector<double> vPves; // proportions of variance explained (R2)
  vector<vector<double> > vvStdSstatsCorr;
  map<string, vector<double> > mUnweightedAbfs;
  map<string, double> mWeightedAbfs;
};

struct Ftr
{
  string name; // eg. ENSG00000182816
  string chr; // eg. chr21
  size_t start; // 1-based coordinate
  size_t end; // idem
  vector<vector<double> > vvPhenos; // phenotypes of samples per subgroup
  vector<vector<bool> > vvIsNa; // missing values per subgroup
  vector<Snp*> vPtCisSnps;
  vector<ResFtrSnp> vResFtrSnps;
  vector<double> vPermPvalsSep; // permutation P-values in each subgroup
  vector<size_t> vNbPermsSoFar; // nb of permutations in each subgroup
  double jointPermPval; // permutation P-value of the joint analysis
  size_t nbPermsSoFar;
  double maxL10TrueAbf;
};

void
Snp_init (
  Snp & iSnp,
  const string & name,
  const size_t & nbSubgroups,
  const size_t & nbSamplesS1)
{
  iSnp.name = name;
  iSnp.chr.clear();
  iSnp.coord = string::npos;
  iSnp.vvGenos.resize (nbSubgroups);
  iSnp.vvIsNa.resize (nbSubgroups);
  iSnp.vMafs = (vector<double> (nbSubgroups, 0.0));
  iSnp.vvGenos[0] = (vector<double> (nbSamplesS1, 0.0));
  iSnp.vvIsNa[0] = (vector<bool> (nbSamplesS1, false));
}

// assume both features are on the same chromosome
bool Snp_compByCoord (
  const Snp* pt_iSnp1,
  const Snp* pt_iSnp2)
{
  bool res = false;
  if (pt_iSnp1->coord < pt_iSnp2->coord)
    res = true;
  return res;
}

int Snp_isInCis (
  const Snp & iSnp,
  const size_t & ftrStart,
  const size_t & ftrEnd,
  const string & anchor,
  const size_t & lenCis)
{
  int res = -1;
  if (anchor.compare("FSS+FES") == 0)
  {
    if (((ftrStart >= lenCis &&
	 iSnp.coord >= ftrStart - lenCis) ||
	(ftrStart < lenCis &&
	 iSnp.coord >= 0)) &&
	iSnp.coord <= ftrEnd + lenCis)
      res = 0;
    else if (iSnp.coord > ftrEnd + lenCis)
      res = 1;
  }
  else if (anchor.compare("FSS") == 0)
  {
    if (((ftrStart >= lenCis &&
	 iSnp.coord >= ftrStart - lenCis) ||
	(ftrStart < lenCis &&
	 iSnp.coord >= 0)) &&
	iSnp.coord <= ftrStart + lenCis)
      res = 0;
    else if (iSnp.coord > ftrStart + lenCis)
      res = 1;
  }
  return res;
}

void
ResFtrSnp_init (
  ResFtrSnp & iResFtrSnp,
  const string & snpName,
  const size_t & nbSubgroups)
{
  iResFtrSnp.snp = snpName;
  iResFtrSnp.vNs.assign (nbSubgroups, 0);
  iResFtrSnp.vBetahats.assign (nbSubgroups,
			       numeric_limits<double>::quiet_NaN());
  iResFtrSnp.vSebetahats.assign (nbSubgroups,
				 numeric_limits<double>::quiet_NaN());
  iResFtrSnp.vSigmahats.assign (nbSubgroups,
				numeric_limits<double>::quiet_NaN());
  iResFtrSnp.vBetaPvals.assign (nbSubgroups,
				numeric_limits<double>::quiet_NaN());
  iResFtrSnp.vPves.assign (nbSubgroups,
			   numeric_limits<double>::quiet_NaN());
}

void
ResFtrSnp_getSstatsOneSbgrpOnePop (
  ResFtrSnp & iResFtrSnp,
  const Ftr & iFtr,
  const Snp & iSnp,
  const size_t & s,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm)
{
  vector<double> y, g;
  size_t idxPheno, idxGeno;
  for (size_t i = 0; i < vvSampleIdxPhenos[s].size(); ++i)
  {
    idxPheno = vvSampleIdxPhenos[s][i];
    idxGeno = vvSampleIdxGenos[0][i];
    if (idxPheno != string::npos
	&& idxGeno != string::npos
	&& ! iFtr.vvIsNa[s][idxPheno]
	&& ! iSnp.vvIsNa[0][idxGeno])
    {
      y.push_back (iFtr.vvPhenos[s][idxPheno]);
      g.push_back (iSnp.vvGenos[0][idxGeno]);
    }
  }
  
  if (needQnorm)
    qqnorm (&y[0], y.size());
  
  iResFtrSnp.vNs[s] = y.size();
  
  if (iResFtrSnp.vNs[s] > 1)
    ols (g, y, iResFtrSnp.vBetahats[s], iResFtrSnp.vSebetahats[s],
	 iResFtrSnp.vSigmahats[s], iResFtrSnp.vBetaPvals[s],
	 iResFtrSnp.vPves[s]);
}

void
ResFtrSnp_getSstatsPermOneSbgrpOnePop (
  ResFtrSnp & iResFtrSnp,
  const Ftr & iFtr,
  const Snp & iSnp,
  const size_t & s,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm,
  const gsl_permutation * perm)
{
  vector<double> y, g;
  size_t idxPheno, idxGeno, p;
  for (size_t i = 0; i < vvSampleIdxPhenos[s].size(); ++i)
  {
    p = gsl_permutation_get (perm, i);
    idxPheno = vvSampleIdxPhenos[s][p];
    idxGeno = vvSampleIdxGenos[0][i];
    if (idxPheno != string::npos
	&& idxGeno != string::npos
	&& ! iFtr.vvIsNa[s][idxPheno]
	&& ! iSnp.vvIsNa[0][idxGeno])
    {
      y.push_back (iFtr.vvPhenos[s][idxPheno]);
      g.push_back (iSnp.vvGenos[0][idxGeno]);
    }
  }
  
  if (needQnorm)
    qqnorm (&y[0], y.size());
  
  iResFtrSnp.vNs[s] = y.size();
  
  if (iResFtrSnp.vNs[s] > 1)
    ols (g, y, iResFtrSnp.vBetahats[s], iResFtrSnp.vSebetahats[s],
	 iResFtrSnp.vSigmahats[s], iResFtrSnp.vBetaPvals[s],
	 iResFtrSnp.vPves[s]);
}

void
ResFtrSnp_corrSmallSampleSize (
  ResFtrSnp & iResFtrSnp)
{
  for (size_t s = 0; s < iResFtrSnp.vNs.size(); ++s)
  {
    if (iResFtrSnp.vNs[s] > 1)
    {
      double n = iResFtrSnp.vNs[s],
	bhat = iResFtrSnp.vBetahats[s] / iResFtrSnp.vSigmahats[s],
	sebhat = iResFtrSnp.vSebetahats[s] / iResFtrSnp.vSigmahats[s],
	t = gsl_cdf_gaussian_Pinv (gsl_cdf_tdist_P (-fabs(bhat/sebhat),
						    n-2), 1.0),
	sigmahat = 0;
      vector<double> vStdSstatsCorr;
      if (fabs(t) > 1e-8)
      {
	sigmahat = fabs (iResFtrSnp.vBetahats[s]) / (fabs (t) * sebhat);
	bhat = iResFtrSnp.vBetahats[s] / sigmahat;
	sebhat = bhat / t;
      }
      else
      {
	sigmahat = numeric_limits<double>::quiet_NaN();
	bhat = 0;
	sebhat = numeric_limits<double>::infinity();
      }
      vStdSstatsCorr.push_back (bhat);
      vStdSstatsCorr.push_back (sebhat);
      vStdSstatsCorr.push_back (t);
      iResFtrSnp.vvStdSstatsCorr.push_back (vStdSstatsCorr);
    }
    else
      iResFtrSnp.vvStdSstatsCorr.push_back (vector<double> (3, 0.0));
  }
}

double
getAbfFromStdSumStats (
  const vector<size_t> & vNs,
  const vector<vector<double> > & vvStdSstatsCorr,
  const double & phi2,
  const double & oma2)
{
  double l10AbfAll = 0, bhat = 0, varbhat = 0, t = 0, bbarhat_num = 0,
    bbarhat_denom = 0, varbbarhat = 0;
  vector<double> l10AbfsSingleSbgrp;
#ifdef DEBUG
  printf ("phi2=%f oma2=%f\n", phi2, oma2);
#endif
  
  for (size_t s = 0; s < vNs.size(); ++s)
  {
    if (vNs[s] > 1)
    {
      bhat = vvStdSstatsCorr[s][0];
      varbhat = pow (vvStdSstatsCorr[s][1], 2);
      t = vvStdSstatsCorr[s][2];
      double lABF_single;
      if (fabs(t) < 1e-8)
      {
	lABF_single = 0;
      }
      else
      {
	bbarhat_num += bhat / (varbhat + phi2);
	bbarhat_denom += 1 / (varbhat + phi2);
	varbbarhat += 1 / (varbhat + phi2);
	lABF_single = 0.5 * log10(varbhat)
	  - 0.5 * log10(varbhat + phi2)
	  + (0.5 * pow(t,2) * phi2 / (varbhat + phi2)) / log(10);
      }
      l10AbfsSingleSbgrp.push_back (lABF_single);
#ifdef DEBUG
      printf ("l10AbfsSingleSbgrp[%ld]=%e\n", s+1, l10AbfsSingleSbgrp[s]);
#endif
    }
  }
  
  double bbarhat = (bbarhat_denom != 0) ?
    bbarhat_num / bbarhat_denom
    : 0;
  varbbarhat = (varbbarhat != 0) ?
    1 / varbbarhat
    : numeric_limits<double>::infinity();
  double T2 = pow(bbarhat, 2.0) / varbbarhat;
  double lABF_bbar = (T2 != 0) ?
    0.5 * log10(varbbarhat) - 0.5 * log10(varbbarhat + oma2)
    + (0.5 * T2 * oma2 / (varbbarhat + oma2)) / log(10)
    : 0;
#ifdef DEBUG
  printf ("bbarhat=%e varbbarhat=%e T2=%e lABF_bbar=%e\n",
	  bbarhat, varbbarhat, T2, lABF_bbar);
#endif
  
  l10AbfAll = lABF_bbar;
  for (size_t i = 0; i < l10AbfsSingleSbgrp.size(); ++i)
    l10AbfAll += l10AbfsSingleSbgrp[i];
#ifdef DEBUG
  printf ("l10AbfAll=%e\n", l10AbfAll);
#endif
  
  return l10AbfAll;
}

void
ResFtrSnp_calcAbfsDefault (
  ResFtrSnp & iResFtrSnp,
  const vector< vector<double> > & grid)
{
  vector<double> vL10AbfsConst (grid.size(), 0.0),
    vL10AbfsConstFix (grid.size(), 0.0),
    vL10AbfsConstMaxh (grid.size(), 0.0);
  for (size_t gridIdx = 0; gridIdx < grid.size(); ++gridIdx)
  {
    vL10AbfsConst[gridIdx] = (getAbfFromStdSumStats (
			       iResFtrSnp.vNs,
			       iResFtrSnp.vvStdSstatsCorr,
			       grid[gridIdx][0],
			       grid[gridIdx][1]));
    vL10AbfsConstFix[gridIdx] = (getAbfFromStdSumStats (
				   iResFtrSnp.vNs,
				   iResFtrSnp.vvStdSstatsCorr,
				   0,
				   grid[gridIdx][0]
				   + grid[gridIdx][1]));
    vL10AbfsConstMaxh[gridIdx] = (getAbfFromStdSumStats (
				    iResFtrSnp.vNs,
				    iResFtrSnp.vvStdSstatsCorr,
				    grid[gridIdx][0]
				    + grid[gridIdx][1],
				    0));
  }
  iResFtrSnp.mUnweightedAbfs.insert (make_pair ("const", vL10AbfsConst));
  iResFtrSnp.mUnweightedAbfs.insert (make_pair ("const-fix", vL10AbfsConstFix));
  iResFtrSnp.mUnweightedAbfs.insert (make_pair ("const-maxh", vL10AbfsConstMaxh));
  
  vector<double> vWeights (grid.size(), 1.0 / (double) grid.size());
  iResFtrSnp.mWeightedAbfs.insert (make_pair ("const",
					      log10_weighted_sum (
						&(vL10AbfsConst[0]),
						&(vWeights[0]),
						grid.size())));
  iResFtrSnp.mWeightedAbfs.insert (make_pair ("const-fix",
					      log10_weighted_sum (
						&(vL10AbfsConstFix[0]),
						&(vWeights[0]),
						grid.size())));
  iResFtrSnp.mWeightedAbfs.insert (make_pair ("const-maxh",
					      log10_weighted_sum (
						&(vL10AbfsConstMaxh[0]),
						&(vWeights[0]),
						grid.size())));
}

void
ResFtrSnp_calcAbfsSpecific (
  ResFtrSnp & iResFtrSnp,
  const vector< vector<double> > & grid)
{
  stringstream ssConfig;
  vector<size_t> vNs (iResFtrSnp.vNs.size(), 0);
  vector< vector<double> > vvStdSstatsCorr;
  vector<double> vL10Abfs;
  vector<double> vWeights (grid.size(), 1.0 / (double) grid.size());
  
  for (size_t s = 0; s < iResFtrSnp.vNs.size(); ++s)
  {
    ssConfig.str("");
    ssConfig << (s+1);
    
    if (iResFtrSnp.vNs[s] > 1)
    {
      vvStdSstatsCorr.clear ();
      for (size_t i = 0; i < iResFtrSnp.vNs.size(); ++i)
      {
	if (s == i)
	{
	  vNs[i] = iResFtrSnp.vNs[i];
	  vvStdSstatsCorr.push_back (iResFtrSnp.vvStdSstatsCorr[i]);
	}
	else
	{
	  vNs[i] = 0;
	  vvStdSstatsCorr.push_back (vector<double> (3, 0));
	}
      }
      
      vL10Abfs.assign (grid.size(), 0);
      for (size_t gridIdx = 0; gridIdx < grid.size(); ++gridIdx)
	vL10Abfs[gridIdx] = getAbfFromStdSumStats (vNs,
						  vvStdSstatsCorr,
						  grid[gridIdx][0],
						  grid[gridIdx][1]);
      iResFtrSnp.mUnweightedAbfs.insert (make_pair (ssConfig.str(), vL10Abfs));
      
      iResFtrSnp.mWeightedAbfs.insert (make_pair(ssConfig.str(),
						 log10_weighted_sum (
						   &(vL10Abfs[0]),
						   &(vWeights[0]),
						   grid.size())));
    }
    else
    {
      iResFtrSnp.mUnweightedAbfs.insert (make_pair (ssConfig.str(),
						    vector<double> (grid.size(),
								    numeric_limits<double>::quiet_NaN())));
      iResFtrSnp.mWeightedAbfs.insert (make_pair(ssConfig.str(),
						 numeric_limits<double>::quiet_NaN()));
    }
  }
}

void
prepareConfig (
  stringstream & ssConfig,
  vector<bool> & vIsEqtlInConfig,
  const gsl_combination * comb)
{
  ssConfig.str("");
  vIsEqtlInConfig.clear();
  
  vIsEqtlInConfig.assign (comb->n, false);
  
  ssConfig << gsl_combination_get (comb, 0) + 1;
  vIsEqtlInConfig[gsl_combination_get (comb, 0)] = true;
  if (comb->k > 1)
  {
    for (size_t i = 1; i < comb->k; ++i)
    {
      ssConfig << "-" << gsl_combination_get (comb, i) + 1;
      vIsEqtlInConfig[gsl_combination_get (comb, i)] = true;
    }
  }
}

void
ResFtrSnp_calcAbfsAllConfigs (
  ResFtrSnp & iResFtrSnp,
  const vector<vector<double> > & grid)
{
  gsl_combination * comb;
  stringstream ssConfig;
  vector<bool> vIsEqtlInConfig; // T,T,F if S=3 and config="1-2"
  vector<size_t> vNs (iResFtrSnp.vNs.size(), 0);
  vector<vector<double> > vvStdSstatsCorr;
  vector<double> vL10Abfs;
  
  for (size_t k = 1; k < iResFtrSnp.vNs.size(); ++k) // <: skip full config
  {
    comb = gsl_combination_calloc (iResFtrSnp.vNs.size(), k);
    if (comb == NULL)
    {
      cerr << "ERROR: can't allocate memory for the combination" << endl;
      exit (1);
    }
    while (true)
    {
      prepareConfig (ssConfig, vIsEqtlInConfig, comb);
      vvStdSstatsCorr.clear();
      for (size_t s = 0; s < iResFtrSnp.vNs.size(); ++s)
      {
	if (iResFtrSnp.vNs[s] > 1 && vIsEqtlInConfig[s])
	{
	  vNs[s] = iResFtrSnp.vNs[s];
	  vvStdSstatsCorr.push_back (iResFtrSnp.vvStdSstatsCorr[s]);
	}
	else
	{
	  vNs[s] = 0;
	  vvStdSstatsCorr.push_back (vector<double> (3, 0));
	}
      }
      if (accumulate (vNs.begin(), vNs.end(), 0) > 0)
      {
	vL10Abfs.assign (grid.size(), 0);
	for (size_t gridIdx = 0; gridIdx < grid.size(); ++gridIdx)
	  vL10Abfs[gridIdx] = getAbfFromStdSumStats (vNs,
						     vvStdSstatsCorr,
						     grid[gridIdx][0],
						     grid[gridIdx][1]);
	iResFtrSnp.mUnweightedAbfs.insert (make_pair (ssConfig.str(),
						      vL10Abfs));
	iResFtrSnp.mWeightedAbfs.insert (make_pair(ssConfig.str(),
						   log10_weighted_sum (
						     &(vL10Abfs[0]),
						     grid.size())));
      }
      else
      {
	iResFtrSnp.mUnweightedAbfs.insert (make_pair(ssConfig.str(),
						     vector<double> (grid.size(),
								     numeric_limits<double>::quiet_NaN())));
	iResFtrSnp.mWeightedAbfs.insert (make_pair(ssConfig.str(),
						   numeric_limits<double>::quiet_NaN()));
      }
      if (gsl_combination_next (comb) != GSL_SUCCESS)
	break;
    }
    gsl_combination_free (comb);
  }
}

void
ResFtrSnp_calcAbfs (
  ResFtrSnp & iResFtrSnp,
  const string & whichBfs,
  const vector< vector<double> > & grid)
{
  ResFtrSnp_corrSmallSampleSize (iResFtrSnp);
  
  ResFtrSnp_calcAbfsDefault (iResFtrSnp, grid);
  
  if (whichBfs.compare("subset") == 0)
    ResFtrSnp_calcAbfsSpecific (iResFtrSnp, grid);
  else if (whichBfs.compare("all") == 0)
    ResFtrSnp_calcAbfsAllConfigs (iResFtrSnp, grid);
}

double
ResFtrSnp_calcAbfConst (
  ResFtrSnp & iResFtrSnp,
  const vector< vector<double> > & grid)
{
  vector<double> vL10AbfsConst (grid.size(), 0.0);
  for (size_t gridIdx = 0; gridIdx < grid.size(); ++gridIdx)
    vL10AbfsConst[gridIdx] = (getAbfFromStdSumStats (
				iResFtrSnp.vNs,
				iResFtrSnp.vvStdSstatsCorr,
				grid[gridIdx][0],
				grid[gridIdx][1]));
  
  vector<double> vWeights (grid.size(), 1.0 / (double) grid.size());
  return log10_weighted_sum (&(vL10AbfsConst[0]),
			     &(vWeights[0]),
			     grid.size());
}

double
ResFtrSnp_calcAbfSubset (
  ResFtrSnp & iResFtrSnp,
  const vector< vector<double> > & grid)
{
  vector<double> vL10Abfs (1+iResFtrSnp.vNs.size(), 0);
  
  vL10Abfs[0] = ResFtrSnp_calcAbfConst (iResFtrSnp, grid);
  
  ResFtrSnp_calcAbfsSpecific (iResFtrSnp, grid);
  size_t i = 1;
  for (map<string, double>::const_iterator it = iResFtrSnp.mWeightedAbfs.begin();
       it != iResFtrSnp.mWeightedAbfs.end(); ++it)
  {
    vL10Abfs[i] = it->second;
    ++i;
  }
  
  vector<double> vWeights (vL10Abfs.size(), 1.0 / (double) vL10Abfs.size());
  // TODO: use better weights
  
  return log10_weighted_sum (&(vL10Abfs[0]),
			     &(vWeights[0]),
			     vL10Abfs.size());
}

double
ResFtrSnp_calcAbfAll (
  ResFtrSnp & iResFtrSnp,
  const vector< vector<double> > & grid)
{
  vector<double> vL10Abfs;
  
  vL10Abfs.push_back (ResFtrSnp_calcAbfConst (iResFtrSnp, grid));
  
  ResFtrSnp_calcAbfsAllConfigs (iResFtrSnp, grid);
  for (map<string, double>::const_iterator it = iResFtrSnp.mWeightedAbfs.begin();
       it != iResFtrSnp.mWeightedAbfs.end(); ++it)
    vL10Abfs.push_back (it->second);
  
  vector<double> vWeights (vL10Abfs.size(), 1.0 / (double) vL10Abfs.size());
  // TODO: use better weights
  
  return log10_weighted_sum (&(vL10Abfs[0]),
			     &(vWeights[0]),
			     vL10Abfs.size());
}

void
Ftr_init (
  Ftr & iFtr,
  const string & name,
  const size_t & nbSubgroups)
{
  iFtr.name = name;
  iFtr.chr.clear();
  iFtr.start = string::npos;
  iFtr.end = string::npos;
  iFtr.vvPhenos.resize (nbSubgroups);
  iFtr.vvIsNa.resize (nbSubgroups);
  for (size_t s = 0; s < nbSubgroups; ++s)
  {
    iFtr.vvPhenos[s] = (vector<double> ());
    iFtr.vvIsNa[s] = (vector<bool> ());
  }
  iFtr.vPermPvalsSep.assign (nbSubgroups, numeric_limits<double>::quiet_NaN());
  iFtr.vNbPermsSoFar.assign (nbSubgroups, 0);
  iFtr.jointPermPval = numeric_limits<double>::quiet_NaN();
  iFtr.nbPermsSoFar = 0;
  iFtr.maxL10TrueAbf = 0;
}

// assume both features are on the same chromosome
bool Ftr_compByCoord (
  const Ftr* pt_iFtr1,
  const Ftr* pt_iFtr2)
{
  bool res = false;
  if ((pt_iFtr1->start < pt_iFtr2->start) ||
      (pt_iFtr1->start == pt_iFtr2->start && 
       pt_iFtr1->end < pt_iFtr2->end ))
      res = true;
  return res;
}

void
Ftr_getCisSnps (
  Ftr & iFtr,
  const map<string, vector<Snp*> > & mChr2VecPtSnps,
  const string & anchor,
  const size_t & lenCis)
{
  map<string, vector<Snp*> >::const_iterator itVecPtSnps =
    mChr2VecPtSnps.find(iFtr.chr);
  
  for (size_t snpId = 0; snpId < itVecPtSnps->second.size(); ++snpId)
  {
    Snp * ptSnp = (itVecPtSnps->second)[snpId];
    int inCis = Snp_isInCis (*ptSnp, iFtr.start, iFtr.end,
			     anchor, lenCis);
    if (inCis == 1)
      break;
    else if (inCis == -1)
      continue;
    iFtr.vPtCisSnps.push_back ((itVecPtSnps->second)[snpId]);
  }
}

void
Ftr_inferAssos (
  Ftr & iFtr,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const int & whichStep,
  const bool & needQnorm,
  const vector<vector<double> > & grid,
  const string & whichBfs,
  const int & verbose)
{
  size_t nbSubgroups = iFtr.vvPhenos.size();
  for (size_t snpId = 0; snpId < iFtr.vPtCisSnps.size(); ++snpId)
  {
    ResFtrSnp iResFtrSnp;
    ResFtrSnp_init (iResFtrSnp, iFtr.vPtCisSnps[snpId]->name, nbSubgroups);
    for (size_t s = 0; s < nbSubgroups; ++s)
      if (iFtr.vvPhenos[s].size() > 0)
	ResFtrSnp_getSstatsOneSbgrpOnePop (iResFtrSnp, iFtr,
					   *(iFtr.vPtCisSnps[snpId]), s,
					   vvSampleIdxPhenos, vvSampleIdxGenos,
					   needQnorm);
    if (whichStep == 3 || whichStep == 4 || whichStep == 5)
      ResFtrSnp_calcAbfs (iResFtrSnp, whichBfs, grid);
    iFtr.vResFtrSnps.push_back (iResFtrSnp);
  }
}

double
Ftr_getMinTrueBetaPvals (
  const Ftr & iFtr,
  const size_t & s)
{
  double minTrueBetaPval = 1;
  for (vector<ResFtrSnp>::const_iterator it = iFtr.vResFtrSnps.begin();
       it != iFtr.vResFtrSnps.end(); ++it)
    if (it->vNs[s] > 1 && it->vBetaPvals[s] < minTrueBetaPval)
      minTrueBetaPval = it->vBetaPvals[s];
  return minTrueBetaPval;
}

void
Ftr_makePermsSepOneSubgrpOnePop (
  Ftr & iFtr,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm,
  const size_t & nbPerms,
  const int & trick,
  const size_t & s,
  const gsl_rng * & rngPerm,
  const gsl_rng * & rngTrick)
{
//  clock_t timeBegin = clock();
  size_t idxPheno, idxGeno, p;
  bool shuffleOnly;
  double minTrueBetaPval, minPermBetaPval, permBetaPval,
    betahat, sebetahat, sigmahat, pve;
  gsl_permutation * perm = NULL;
  Snp iSnp;
  
  iFtr.vPermPvalsSep[s] = 1;
  iFtr.vNbPermsSoFar[s] = 0;
  minTrueBetaPval = Ftr_getMinTrueBetaPvals (iFtr, s);
  shuffleOnly = false;
  
  perm = gsl_permutation_calloc (vvSampleIdxPhenos[s].size());
  if (perm == NULL)
  {
    cerr << "ERROR: can't allocate memory for the permutation" << endl;
    exit (1);
  }
  
  for(size_t permId = 0; permId < nbPerms; ++permId)
  {
    gsl_ran_shuffle (rngPerm, perm->data, perm->size, sizeof(size_t));
    if (shuffleOnly)
      continue;
    
    ++(iFtr.vNbPermsSoFar[s]);
    minPermBetaPval = 1;
    
    for (size_t snpId = 0; snpId < iFtr.vPtCisSnps.size(); ++snpId)
    {
      iSnp = *(iFtr.vPtCisSnps[snpId]);
      permBetaPval = 1;
      vector<double> y, g;
      for (size_t i = 0; i < vvSampleIdxPhenos[s].size(); ++i)
      {
	p = gsl_permutation_get (perm, i);
	idxPheno = vvSampleIdxPhenos[s][p];
	idxGeno = vvSampleIdxGenos[0][i];
	if (idxPheno != string::npos
	    && idxGeno != string::npos
	    && ! iFtr.vvIsNa[s][idxPheno]
	    && ! iSnp.vvIsNa[0][idxGeno])
	{
	  y.push_back (iFtr.vvPhenos[s][idxPheno]);
	  g.push_back (iSnp.vvGenos[0][idxGeno]);
	}
      }
      if (needQnorm)
	qqnorm (&y[0], y.size());
      if (y.size() > 1)
	ols (g, y, betahat, sebetahat, sigmahat, permBetaPval,
	     pve);
      if (permBetaPval < minPermBetaPval)
	minPermBetaPval = permBetaPval;
    }
    
    if (minPermBetaPval <= minTrueBetaPval)
      ++(iFtr.vPermPvalsSep[s]);
    if (trick != 0 && iFtr.vPermPvalsSep[s] == 11)
    {
      if (trick == 1)
	break;
      else if (trick == 2)
	shuffleOnly = true;
    }
  }
  
  if (iFtr.vNbPermsSoFar[s] == nbPerms)
    iFtr.vPermPvalsSep[s] /= (nbPerms + 1);
  else
    iFtr.vPermPvalsSep[s] = gsl_ran_flat (rngTrick,
					  (11 / ((double) (iFtr.vNbPermsSoFar[s] + 2))),
					  (11 / ((double) (iFtr.vNbPermsSoFar[s] + 1))));
  
  gsl_permutation_free (perm);
//  cout << endl << setprecision(8) << (clock() - timeBegin) / (double(CLOCKS_PER_SEC)*60.0) << endl;
}

/** \brief Retrieve the highest log10(ABF) over SNPs of the given feature
 *  among the const ABF
 */
double
Ftr_getMaxL10TrueAbfConst (
  const Ftr & iFtr)
{
  double maxL10TrueAbf = 0;
  
  // loop over SNPs
  for (vector<ResFtrSnp>::const_iterator it = iFtr.vResFtrSnps.begin();
       it != iFtr.vResFtrSnps.end(); ++it)
    if ((it->mWeightedAbfs.find("const"))->second > maxL10TrueAbf)
      maxL10TrueAbf = (it->mWeightedAbfs.find("const"))->second;
  
  return maxL10TrueAbf;
}

/** \brief Retrieve the highest log10(ABF) over SNPs of the given feature
 *  among the const ABF and each subgroup-specific ABF
 */
double
Ftr_getMaxL10TrueAbfSubset (
  const Ftr & iFtr)
{
  double maxL10TrueAbf = 0;
  stringstream ssConfig;
  
  // loop over SNPs
  for (vector<ResFtrSnp>::const_iterator it = iFtr.vResFtrSnps.begin();
       it != iFtr.vResFtrSnps.end(); ++it)
  {
    if ((it->mWeightedAbfs.find("const"))->second > maxL10TrueAbf)
      maxL10TrueAbf = (it->mWeightedAbfs.find("const"))->second;
    
    // loop over subgroup-specific configuration
    for (size_t s = 0; s < it->vNs.size(); ++s)
    {
      ssConfig.str("");
      ssConfig << (s+1);
      if (it->mWeightedAbfs.find(ssConfig.str())->second > maxL10TrueAbf)
	maxL10TrueAbf = it->mWeightedAbfs.find(ssConfig.str())->second;
    }
  }
  
  return maxL10TrueAbf;
}

/** \brief Retrieve the highest log10(ABF) over SNPs of the given feature
 *  among the const ABF, each subgroup-specific ABF, and all other
 *  configurations
 */
double
Ftr_getMaxL10TrueAbfAll (
  const Ftr & iFtr)
{
  double maxL10TrueAbf = 0;
  stringstream ssConfig;
  gsl_combination * comb;
  vector<bool> vIsEqtlInConfig;
  
  // loop over SNPs
  for (vector<ResFtrSnp>::const_iterator it = iFtr.vResFtrSnps.begin();
       it != iFtr.vResFtrSnps.end(); ++it)
  {
    if ((it->mWeightedAbfs.find("const"))->second > maxL10TrueAbf)
      maxL10TrueAbf = (it->mWeightedAbfs.find("const"))->second;
    
    // loop over all configurations (except const)
    for (size_t k = 1; k < it->vNs.size(); ++k)
    {
      comb = gsl_combination_calloc (it->vNs.size(), k);
      if (comb == NULL)
      {
	cerr << "ERROR: can't allocate memory for the combination" << endl;
	exit (1);
      }
      while (true)
      {
	prepareConfig (ssConfig, vIsEqtlInConfig, comb);
	if (it->mWeightedAbfs.find(ssConfig.str())->second > maxL10TrueAbf)
	  maxL10TrueAbf = it->mWeightedAbfs.find(ssConfig.str())->second;
      }
    }
  }
  
  return maxL10TrueAbf;
}

void
Ftr_makePermsJointOnePopAbfConst (
  Ftr & iFtr,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm,
  const vector<vector<double> > & grid,
  const size_t & nbPerms,
  const int & trick,
  const double & maxL10TrueAbf,
  const gsl_rng * & rngPerm,
  const gsl_rng * & rngTrick,
  const gsl_permutation * perm)
{
  size_t nbSubgroups = iFtr.vvPhenos.size();
  double maxL10PermAbf, l10Abf;
  bool shuffleOnly = false;
  Snp iSnp;
  
  for(size_t permId = 0; permId < nbPerms; ++permId)
  {
    gsl_ran_shuffle (rngPerm, perm->data, perm->size, sizeof(size_t));
    if (shuffleOnly)
      continue;
    
    ++iFtr.nbPermsSoFar;
    maxL10PermAbf = 0;
    
    for (size_t snpId = 0; snpId < iFtr.vPtCisSnps.size(); ++snpId)
    {
      iSnp = *(iFtr.vPtCisSnps[snpId]);
      ResFtrSnp iResFtrSnp;
      ResFtrSnp_init (iResFtrSnp, iSnp.name, nbSubgroups);
      for (size_t s = 0; s < nbSubgroups; ++s)
	if (iFtr.vvPhenos[s].size() > 0)
	  ResFtrSnp_getSstatsPermOneSbgrpOnePop (iResFtrSnp, iFtr, iSnp, s,
						 vvSampleIdxPhenos,
						 vvSampleIdxGenos,
						 needQnorm, perm);
      ResFtrSnp_corrSmallSampleSize (iResFtrSnp);
      l10Abf = ResFtrSnp_calcAbfConst (iResFtrSnp, grid);
      if (l10Abf > maxL10PermAbf)
	maxL10PermAbf = l10Abf;
    }
    
    if (maxL10PermAbf >= maxL10TrueAbf)
    {
      ++iFtr.jointPermPval;
//      cout << endl << permId << " " << maxL10PermAbf << endl;
    }
    if (trick != 0 && iFtr.jointPermPval == 11)
    {
      if (trick == 1)
	break;
      else if (trick == 2)
	shuffleOnly = true;
    }
  }
}

void
Ftr_makePermsJointOnePopAbfSubset (
  Ftr & iFtr,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm,
  const vector<vector<double> > & grid,
  const size_t & nbPerms,
  const int & trick,
  const double & maxL10TrueAbf,
  const gsl_rng * & rngPerm,
  const gsl_rng * & rngTrick,
  const gsl_permutation * perm)
{
  size_t nbSubgroups = iFtr.vvPhenos.size();
  double maxL10PermAbf, l10Abf;
  bool shuffleOnly = false;
  Snp iSnp;
  
  for(size_t permId = 0; permId < nbPerms; ++permId)
  {
    gsl_ran_shuffle (rngPerm, perm->data, perm->size, sizeof(size_t));
    if (shuffleOnly)
      continue;
    
    ++iFtr.nbPermsSoFar;
    maxL10PermAbf = 0;
    
    for (size_t snpId = 0; snpId < iFtr.vPtCisSnps.size(); ++snpId)
    {
      iSnp = *(iFtr.vPtCisSnps[snpId]);
      ResFtrSnp iResFtrSnp;
      ResFtrSnp_init (iResFtrSnp, iSnp.name, nbSubgroups);
      for (size_t s = 0; s < nbSubgroups; ++s)
	if (iFtr.vvPhenos[s].size() > 0)
	  ResFtrSnp_getSstatsPermOneSbgrpOnePop (iResFtrSnp, iFtr, iSnp, s,
						 vvSampleIdxPhenos,
						 vvSampleIdxGenos,
						 needQnorm, perm);
      ResFtrSnp_corrSmallSampleSize (iResFtrSnp);
      l10Abf = ResFtrSnp_calcAbfSubset (iResFtrSnp, grid);
      if (l10Abf > maxL10PermAbf)
	maxL10PermAbf = l10Abf;
    }
    
    if (maxL10PermAbf >= maxL10TrueAbf)
      ++iFtr.jointPermPval;
    if (trick != 0 && iFtr.jointPermPval == 11)
    {
      if (trick == 1)
	break;
      else if (trick == 2)
	shuffleOnly = true;
    }
  }
}

void
Ftr_makePermsJointOnePopAbfAll (
  Ftr & iFtr,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm,
  const vector<vector<double> > & grid,
  const size_t & nbPerms,
  const int & trick,
  const double & maxL10TrueAbf,
  const gsl_rng * & rngPerm,
  const gsl_rng * & rngTrick,
  const gsl_permutation * perm)
{
  size_t nbSubgroups = iFtr.vvPhenos.size();
  double maxL10PermAbf, l10Abf;
  bool shuffleOnly = false;
  Snp iSnp;
  
  for(size_t permId = 0; permId < nbPerms; ++permId)
  {
    gsl_ran_shuffle (rngPerm, perm->data, perm->size, sizeof(size_t));
    if (shuffleOnly)
      continue;
    
    ++iFtr.nbPermsSoFar;
    maxL10PermAbf = 0;
    
    for (size_t snpId = 0; snpId < iFtr.vPtCisSnps.size(); ++snpId)
    {
      iSnp = *(iFtr.vPtCisSnps[snpId]);
      ResFtrSnp iResFtrSnp;
      ResFtrSnp_init (iResFtrSnp, iSnp.name, nbSubgroups);
      for (size_t s = 0; s < nbSubgroups; ++s)
	if (iFtr.vvPhenos[s].size() > 0)
	  ResFtrSnp_getSstatsPermOneSbgrpOnePop (iResFtrSnp, iFtr, iSnp, s,
						 vvSampleIdxPhenos,
						 vvSampleIdxGenos,
						 needQnorm, perm);
      ResFtrSnp_corrSmallSampleSize (iResFtrSnp);
      l10Abf = ResFtrSnp_calcAbfAll (iResFtrSnp, grid);
      if (l10Abf > maxL10PermAbf)
	maxL10PermAbf = l10Abf;
    }
    
    if (maxL10PermAbf >= maxL10TrueAbf)
      ++iFtr.jointPermPval;
    if (trick != 0 && iFtr.jointPermPval == 11)
    {
      if (trick == 1)
	break;
      else if (trick == 2)
	shuffleOnly = true;
    }
  }
}

void
Ftr_makePermsJointOnePop (
  Ftr & iFtr,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm,
  const vector<vector<double> > & grid,
  const size_t & nbPerms,
  const int & trick,
  const string & whichPermBf,
  const gsl_rng * & rngPerm,
  const gsl_rng * & rngTrick)
{
//  clock_t timeBegin = clock();
  double maxL10TrueAbf;
  gsl_permutation * perm = NULL;
  
  perm = gsl_permutation_calloc (vvSampleIdxPhenos[0].size());
  if (perm == NULL)
  {
    cerr << "ERROR: can't allocate memory for the permutation" << endl;
    exit (1);
  }
  
  iFtr.jointPermPval = 1;
  iFtr.nbPermsSoFar = 0;
  
  if (whichPermBf.compare("const") == 0)
  {
    maxL10TrueAbf = Ftr_getMaxL10TrueAbfConst (iFtr);
    iFtr.maxL10TrueAbf = maxL10TrueAbf;
    Ftr_makePermsJointOnePopAbfConst (iFtr, vvSampleIdxPhenos,
				      vvSampleIdxGenos, needQnorm, grid,
				      nbPerms, trick, maxL10TrueAbf,
				      rngPerm, rngTrick, perm);
  }
  else if (whichPermBf.compare("subset") == 0)
  {
    maxL10TrueAbf = Ftr_getMaxL10TrueAbfSubset (iFtr);
    Ftr_makePermsJointOnePopAbfSubset (iFtr, vvSampleIdxPhenos,
				       vvSampleIdxGenos, needQnorm, grid,
				       nbPerms, trick, maxL10TrueAbf,
				       rngPerm, rngTrick, perm);
  }
  else if (whichPermBf.compare("all") == 0)
  {
    maxL10TrueAbf = Ftr_getMaxL10TrueAbfAll (iFtr);
    Ftr_makePermsJointOnePopAbfAll (iFtr, vvSampleIdxPhenos,
				    vvSampleIdxGenos, needQnorm, grid,
				    nbPerms, trick, maxL10TrueAbf,
				    rngPerm, rngTrick, perm);
  }
  
  if (iFtr.nbPermsSoFar == nbPerms)
    iFtr.jointPermPval /= (nbPerms + 1);
  else
    iFtr.jointPermPval = gsl_ran_flat (rngTrick,
				       (11 / ((double) (iFtr.nbPermsSoFar + 2))),
				       (11 / ((double) (iFtr.nbPermsSoFar + 1))));
  
  gsl_permutation_free (perm);
//  cout << endl << setprecision(8) << (clock() - timeBegin) / (double(CLOCKS_PER_SEC)*60.0) << endl;
}

void
loadListsGenoAndPheno (
  const string & genoPathsFile,
  const string & phenoPathsFile,
  map<string, string> & mGenoPaths,
  map<string, string> & mPhenoPaths,
  vector<string> & vSubgroups,
  const int & verbose)
{
  loadTwoColumnFile (phenoPathsFile, mPhenoPaths, vSubgroups, verbose);
  
  vector<string> vSubgroupsGeno;
  loadTwoColumnFile (genoPathsFile, mGenoPaths, vSubgroupsGeno, verbose);
  if (mGenoPaths.size() > 1)
  {
    cerr << "ERROR: current version can't handle several genotype files"
	 << endl;
    exit (1);
    
    if (mGenoPaths.size() != mPhenoPaths.size())
    {
      cerr << "ERROR: there should be only one genotype file"
	   << " or as many as phenotype files" << endl;
      exit (1);
    }
    
    for (size_t s = 0; s < vSubgroups.size(); ++s)
      if (vSubgroupsGeno[s].compare(vSubgroups[s]) != 0)
      {
	cerr << "ERROR: subgroups are not in the same order in "
	     << phenoPathsFile << " and " << genoPathsFile << endl;
	exit (1);
      }
  }
}

void
loadSamplesAllPhenos (
  const map<string, string> & mPhenoPaths,
  const vector<string> & vSubgroups,
  vector<string> & vSamples,
  vector<vector<string> > & vvSamples,
  const int & verbose)
{
  ifstream stream;
  string line;
  for (size_t s = 0; s < vSubgroups.size(); ++s)
  {
    openFile (mPhenoPaths.find(vSubgroups[s])->second, stream);
    getline (stream, line);
    stream.close();
    if (s == 0)
    {
      split (line, " \t", vSamples);
      if (vSamples[0].compare("Id") == 0)
	vSamples.erase (vSamples.begin());
      vvSamples.push_back (vSamples);
    }
    else
    {
      vector<string> tokens;
      split (line, " \t", tokens);
      if (tokens[0].compare("Id") == 0)
	tokens.erase (tokens.begin());
      vvSamples.push_back (tokens);
      for (size_t i = 0; i < tokens.size(); ++i)
	if (find (vSamples.begin(), vSamples.end(), tokens[i])
	    == vSamples.end())
	  vSamples.push_back (tokens[i]);
    }
  }
  if (verbose > 0)
  {
    cout << "nb of samples (phenotypes): "
	 << vSamples.size() << endl << flush;
    for (size_t s = 0; s < vSubgroups.size(); ++s)
    {
      cout << "s" << (s+1) << " (" << vSubgroups[s] << "): "
	   << vvSamples[s].size() << " samples" << endl << flush;
      if (verbose > 1)
      {
	for (size_t i = 0; i < vvSamples[s].size(); ++i)
	  cout << vvSamples[s][i] << endl;
      }
    }
  }
}

void
loadSamplesAllGenos (
  const map<string, string> & mGenoPaths,
  const vector<string> & vSubgroups,
  vector<string> & vSamples,
  vector<vector<string> > & vvSamples,
  const int & verbose)
{
  ifstream stream;
  string line, sample;
  vector<string> tokens, tokens2;
  size_t i;
  for (size_t s = 0; s < vSubgroups.size(); ++s)
  {
    openFile (mGenoPaths.find(vSubgroups[s])->second, stream);
    getline (stream, line);
    stream.close();
    split (line, " \t", tokens);
    if ((tokens.size() - 5) % 3 != 0)
    {
      cerr << "ERROR: the header of file "
	   << mGenoPaths.find(vSubgroups[s])->second
	   << " is badly formatted" << endl;
      exit (1);
    }
    tokens2.clear();
    i = 5;
    while (i < tokens.size())
    {
      sample = split (tokens[i], "_a", 0); // indX_a1a1, indX_a1a2 or indX_a2a2
      tokens2.push_back (sample);
      i = i + 3;
    }
    vvSamples.push_back (tokens2);
    for (i = 0; i < tokens2.size(); ++i)
      if (find (vSamples.begin(), vSamples.end(), tokens2[i])
	  == vSamples.end())
	vSamples.push_back (tokens2[i]);
    if (mGenoPaths.size() == 1)
      break;
  }
  if (verbose > 0)
  {
    cout << "nb of samples (genotypes): "
	 << vSamples.size() << endl << flush;
    for (size_t s = 0; s < vSubgroups.size(); ++s)
    {
      cout << "s" << (s+1) << " (" << vSubgroups[s] << "): "
	   << vvSamples[s].size() << " samples" << endl << flush;
      if (verbose > 1)
      {
	for (size_t i = 0; i < vvSamples[s].size(); ++i)
	  cout << vvSamples[s][i] << endl;
      }
      if (mGenoPaths.size() == 1)
	break;
    }
  }
}

void
loadSamples (
  const map<string, string> & mGenoPaths,
  const map<string, string> & mPhenoPaths,
  const vector<string> & vSubgroups,
  vector<string> & vSamples,
  vector<vector<size_t> > & vvSampleIdxGenos,
  vector<vector<size_t> > & vvSampleIdxPhenos,
  const int & verbose)
{
  if (verbose > 0)
    cout << "load samples ..." << endl << flush;
  
  vector<string> vAllSamplesPhenos;
  vector<vector<string> > vvSamplesPhenos;
  loadSamplesAllPhenos (mPhenoPaths, vSubgroups, vAllSamplesPhenos,
			vvSamplesPhenos, verbose);
  
  vector<string> vAllSamplesGenos;
  vector<vector<string> > vvSamplesGenos;
  loadSamplesAllGenos (mGenoPaths, vSubgroups, vAllSamplesGenos,
		       vvSamplesGenos, verbose);
  
  // fill vSamples by merging vAllSamplesPhenos and vAllSamplesGenos
  vector<string>::iterator it;
  for (it = vAllSamplesPhenos.begin();
       it != vAllSamplesPhenos.end(); ++it)
    if (find(vSamples.begin(), vSamples.end(), *it) == vSamples.end())
      vSamples.push_back (*it);
  for (it = vAllSamplesGenos.begin();
       it != vAllSamplesGenos.end(); ++it)
    if (find(vSamples.begin(), vSamples.end(), *it) == vSamples.end())
      vSamples.push_back (*it);
  if (verbose > 0)
    cout << "total nb of samples: "
	 << vSamples.size() << endl << flush;
  
  // fill vvSampleIdxPhenos
  // vvSampleIdxPhenos[3][0] = 5 means that the 1st sample in vSamples
  // corresponds to the 6th sample in the 4th subgroup
  // it is npos if the sample is absent from this subgroup
  for (size_t s = 0; s < vvSamplesPhenos.size(); ++s)
  {
    vector<size_t> vSampleIdxPhenos (vSamples.size(), string::npos);
    for (size_t i = 0; i < vSamples.size(); ++i)
    {
      vector<string>::iterator it = find(vvSamplesPhenos[s].begin(),
					 vvSamplesPhenos[s].end(),
					 vSamples[i]);
      if (it != vvSamplesPhenos[s].end())
	vSampleIdxPhenos[i] = it - vvSamplesPhenos[s].begin();
    }
    vvSampleIdxPhenos.push_back (vSampleIdxPhenos);
  }
  
  // fill vvSampleIdxGenos
  for (size_t s = 0; s < vvSamplesGenos.size(); ++s)
  {
    vector<size_t> vSampleIdxGenos (vSamples.size(), string::npos);
    for (size_t i = 0; i < vSamples.size(); ++i)
    {
      vector<string>::iterator it = find(vvSamplesGenos[s].begin(),
					 vvSamplesGenos[s].end(),
					 vSamples[i]);
      if (it != vvSamplesGenos[s].end())
	vSampleIdxGenos[i] = it - vvSamplesGenos[s].begin();
    }
    vvSampleIdxGenos.push_back (vSampleIdxGenos);
  }
  
  // if (verbose > 0)
  // {
  //   for (size_t s = 0; s < vvSampleIdxPhenos.size(); ++s)
  //     for (size_t i = 0; i < vSamples.size(); ++i)
  // 	cout << "s" << (s+1) << " " << vSamples[i] << ": " << (vvSampleIdxPhenos[s][i]+1) << endl;
  // }
}

void
loadPhenos (
  const map<string, string> & mPhenoPaths,
  const vector<string> & vSubgroups,
  const vector<string> & vFtrsToKeep,
  map<string, Ftr> & mFtrs,
  const int & verbose)
{
  if (verbose > 0)
    cout << "load phenotypes ..." << endl << flush;
  
  ifstream phenoStream;
  string line;
  vector<string> tokens;
  size_t  nbSamples, nbLines;
  
  for (size_t s = 0; s < vSubgroups.size(); ++s)
  {
    openFile (mPhenoPaths.find(vSubgroups[s])->second, phenoStream);
    getline (phenoStream, line); // header
    split (line, " \t", tokens);
    if (tokens[0].compare("Id") == 0)
      nbSamples = tokens.size() - 1;
    else
      nbSamples = tokens.size();
    nbLines = 1;
    
    while (true)
    {
      getline (phenoStream, line);
      if (line.empty())
	break;
      ++nbLines;
      split (line, " \t", tokens);
      if (! vFtrsToKeep.empty()
	  && find (vFtrsToKeep.begin(), vFtrsToKeep.end(), tokens[0])
	  == vFtrsToKeep.end())
	continue;
      if (tokens.size() != nbSamples + 1)
      {
	cerr << "ERROR: not enough columns on line " << nbLines
	     << " of file " << mPhenoPaths.find(vSubgroups[s])->second
	     << endl;
	exit (1);
      }
      
      if (mFtrs.find(tokens[0]) == mFtrs.end())
      {
	Ftr iFtr;
	Ftr_init (iFtr, tokens[0], mPhenoPaths.size());
	iFtr.vvIsNa[s].resize (nbSamples, false);
	iFtr.vvPhenos[s].resize (nbSamples,
				 numeric_limits<double>::quiet_NaN());
	for (size_t i = 1; i < tokens.size(); ++i)
	{
	  if (tokens[i].compare("NA") == 0)
	    iFtr.vvIsNa[s][i-1] = true;
	  else
	    iFtr.vvPhenos[s][i-1] = atof (tokens[i].c_str());
	}
	mFtrs.insert (make_pair (tokens[0], iFtr));
      }
      else
      {
	mFtrs[tokens[0]].vvIsNa[s].resize (nbSamples, false);
	mFtrs[tokens[0]].vvPhenos[s].resize (nbSamples,
					     numeric_limits<double>::quiet_NaN());
	for (size_t i = 1; i < tokens.size() ; ++i)
	{
	  if (tokens[i].compare("NA") == 0)
	    mFtrs[tokens[0]].vvIsNa[s][i-1] = true;
	  else
	    mFtrs[tokens[0]].vvPhenos[s][i-1] = atof (tokens[i].c_str());
	}
      }
    }
    
    phenoStream.close();
  }
  
  if (mFtrs.size() == 0)
  {
    cerr << "ERROR: no feature to analyze" << endl;
    exit (1);
  }
  if (verbose > 0)
    cout << "nb of features: " << mFtrs.size() << endl;
/*	map<string, Ftr>::iterator it = mFtrs.begin();
	while (it != mFtrs.end())
	{
	for (size_t s = 0; s < it->second.vvPhenos.size(); ++s)
	for (size_t i = 0; i < it->second.vvPhenos[s].size(); ++i)
	cout << it->first << " " << (s+1) << " " << (i+1) << " "
	<< it->second.vvPhenos[s][i] << endl;
	++it;
	}*/
}

void
loadFtrInfo (
  const string & ftrCoordsFile,
  map<string, Ftr> & mFtrs,
  map<string, vector<Ftr*> > & mChr2VecPtFtrs,
  const int & verbose)
{
  if (verbose > 0)
    cout << "load feature coordinates ..." << endl << flush;
  
  // parse the BED file
  ifstream ftrCoordsStream;
  openFile (ftrCoordsFile, ftrCoordsStream);
  string line;
  vector<string> tokens;
  while (true)
  {
    getline (ftrCoordsStream, line);
    if (line.empty())
      break;
    split (line, " \t", tokens);
    if (mFtrs.find(tokens[3]) == mFtrs.end())
      continue;
    mFtrs[tokens[3]].chr = tokens[0];
    mFtrs[tokens[3]].start = atol (tokens[1].c_str()) + 1;
    mFtrs[tokens[3]].end = atol (tokens[2].c_str());
    
    if (mChr2VecPtFtrs.find(tokens[0]) == mChr2VecPtFtrs.end())
      mChr2VecPtFtrs.insert (make_pair (tokens[0],
					vector<Ftr*> ()));
    mChr2VecPtFtrs[tokens[0]].push_back (&(mFtrs[tokens[3]]));
  }
  ftrCoordsStream.close();
  
  // check that all features have coordinates
  map<string, Ftr>::iterator it = mFtrs.begin();
  while (it != mFtrs.end())
  {
    if (it->second.chr.empty())
    {
      cerr << "ERROR: some features have no coordinate, eg. "
	   << it->second.name << endl;
      exit (1);
    }
    ++it;
  }
  
  // sort the features per chr
  for (map<string, vector<Ftr*> >::iterator it = mChr2VecPtFtrs.begin();
       it != mChr2VecPtFtrs.end(); ++it)
    sort (it->second.begin(), it->second.end(), Ftr_compByCoord);
}

void
loadGenosAndSnpInfo (
  const map<string, string> & mGenoPaths,
  const vector<string> & vSubgroups,
  const vector<string> & vSnpsToKeep,
  map<string, Snp> & mSnps,
  map<string, vector<Snp*> > & mChr2VecPtSnps,
  const int & verbose)
{
  if (verbose > 0)
    cout << "load genotypes and SNP coordinates ..." << endl << flush;
  
  ifstream genoStream;
  string line;
  vector<string> tokens;
  size_t nbSamples, nbLines;
  double maf, AA, AB, BB;
  
  for (size_t s = 0; s < vSubgroups.size(); ++s)
  {
    openFile (mGenoPaths.find(vSubgroups[s])->second, genoStream);
    getline (genoStream, line); // header
    split (line, " \t", tokens);
    nbSamples = (size_t) (tokens.size() - 5) / 3;
    nbLines = 1;
    
    while (true)
    {
      getline (genoStream, line);
      if (line.empty())
	break;
      ++nbLines;
      split (line, " \t", tokens);
      if (! vSnpsToKeep.empty()
	  && find (vSnpsToKeep.begin(), vSnpsToKeep.end(), tokens[1])
	  == vSnpsToKeep.end())
	continue;
      if (tokens.size() != (size_t) (3 * nbSamples + 5))
      {
	cerr << "ERROR: not enough columns on line " << nbLines
	     << " of file " << mGenoPaths.find(vSubgroups[s])->second
	     << endl;
	exit (1);
      }
      
      if (mSnps.find(tokens[1]) == mSnps.end())
      {
	Snp iSnp;
	Snp_init (iSnp, tokens[1], mGenoPaths.size(), nbSamples);
	maf = 0;
	for (size_t i = 0; i < nbSamples; ++i)
	{
	  AA = atof(tokens[5+3*i].c_str());
	  AB = atof(tokens[5+3*i+1].c_str());
	  BB = atof(tokens[5+3*i+2].c_str());
	  if (AA == 0 && AB == 0 && BB == 0)
	    iSnp.vvIsNa[s][i] = true;
	  else
	  {
	    iSnp.vvGenos[s][i] = 0 * AA + 1 * AB + 2 * BB;
	    maf += iSnp.vvGenos[s][i];
	  }
	}
	maf /= 2 * (nbSamples
		    - count (iSnp.vvIsNa[s].begin(),
			     iSnp.vvIsNa[s].end(),
			     true));
	iSnp.vMafs[s] = maf <= 0.5 ? maf : (1 - maf);
	iSnp.chr = tokens[0];
	iSnp.coord = atol (tokens[2].c_str());
	mSnps.insert (make_pair (tokens[1], iSnp));
	
	if (mChr2VecPtSnps.find(tokens[0]) == mChr2VecPtSnps.end())
	  mChr2VecPtSnps.insert (make_pair (tokens[0],
					    vector<Snp*> ()));
	mChr2VecPtSnps[tokens[0]].push_back (&(mSnps[tokens[1]]));
      }
    }
    
    genoStream.close();
    if (mGenoPaths.size() == 1)
      break;
  }
  
  for (map<string, Snp>::iterator it = mSnps.begin();
       it != mSnps.end(); ++it)
    if (it->second.vvGenos.size() == 0)
    {
      cerr << "ERROR: SNP " << it->first << " has no genotype" << endl;
      exit (1);
    }
  
  // sort the SNPs per chr
  for (map<string, vector<Snp*> >::iterator it = mChr2VecPtSnps.begin();
       it != mChr2VecPtSnps.end(); ++it)
    sort (it->second.begin(), it->second.end(), Snp_compByCoord);
  
  if (verbose > 0)
    cout << "nb of SNPs: " << mSnps.size() << endl;
}

void
inferAssos (
  map<string, Ftr> & mFtrs,
  const map<string, vector<Ftr*> > & mChr2VecPtFtrs,
  const map<string, Snp> & mSnps,
  const map<string, vector<Snp*> > & mChr2VecPtSnps,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const string & anchor,
  const size_t & lenCis,
  const int & whichStep,
  const bool & needQnorm,
  const vector<vector<double> > & grid,
  const string & whichBfs,
  const int & verbose)
{
  if (verbose > 0)
    cout << "look for association between each pair feature-SNP ..." << endl
	 << "anchor=" << anchor << " lenCis=" << lenCis << endl << flush;
  
  size_t nbAnalyzedPairs = 0;
  
  size_t countFtrs = 0;
  for (map<string, Ftr>::iterator itF = mFtrs.begin();
       itF != mFtrs.end(); ++itF)
  {
    ++countFtrs;
    Ftr_getCisSnps (itF->second, mChr2VecPtSnps, anchor, lenCis);
    if (itF->second.vPtCisSnps.size() > 0)
    {
      if (verbose == 1)
	progressBar ("", countFtrs, mFtrs.size());
      if (verbose > 1)
	cout << itF->second.name << ": " << itF->second.vPtCisSnps.size()
	     << " SNPs in cis" << endl << flush;
      Ftr_inferAssos (itF->second, vvSampleIdxPhenos, vvSampleIdxGenos,
		      whichStep, needQnorm, grid, whichBfs, verbose-1);
      nbAnalyzedPairs += itF->second.vResFtrSnps.size();
    }
  }
  
  if (verbose > 0)
    cout << endl << "nb of analyzed feature-SNP pairs: " << nbAnalyzedPairs << endl;
}

void
makePermsSepOnePop (
  map<string, Ftr> & mFtrs,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm,
  const size_t & nbPerms,
  const size_t & seed,
  const int & trick,
  const gsl_rng * rngPerm,
  const gsl_rng * rngTrick,
  const int & verbose)
{
  size_t nbSubgroups = vvSampleIdxPhenos.size();
  stringstream ss;
  
  for (size_t s = 0; s < nbSubgroups; ++s)
  {
    gsl_rng_set (rngPerm, seed);
    if (trick != 0)
      gsl_rng_set (rngTrick, seed);
    ss.str("");
    ss << "s" << (s+1);
    
    size_t countFtrs = 0;
    for (map<string, Ftr>::iterator itF = mFtrs.begin();
	 itF != mFtrs.end(); ++itF)
    {
      ++countFtrs;
      if (itF->second.vPtCisSnps.size() > 0)
      {
	if (verbose == 1)
	  progressBar (ss.str(), countFtrs, mFtrs.size());
	Ftr_makePermsSepOneSubgrpOnePop (itF->second, vvSampleIdxPhenos,
					 vvSampleIdxGenos, needQnorm, nbPerms, trick,
					 s, rngPerm, rngTrick);
      }
    }
    if (verbose == 1)
      cout << endl << flush;
  }
}

void
makePermsJointOnePop (
  map<string, Ftr> & mFtrs,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const bool & needQnorm,
  const vector<vector<double> > & grid,
  const size_t & nbPerms,
  const size_t & seed,
  const int & trick,
  const string & whichPermBf,
  const gsl_rng * rngPerm,
  const gsl_rng * rngTrick,
  const int & verbose)
{
  gsl_rng_set (rngPerm, seed);
  if (trick != 0)
    gsl_rng_set (rngTrick, seed);
  size_t countFtrs = 0;
  for (map<string, Ftr>::iterator itF = mFtrs.begin();
       itF != mFtrs.end(); ++itF)
  {
    ++countFtrs;
    if (itF->second.vPtCisSnps.size() > 0)
    {
      if (verbose == 1)
	progressBar ("joint", countFtrs, mFtrs.size());
      Ftr_makePermsJointOnePop (itF->second, vvSampleIdxPhenos,
				vvSampleIdxGenos, needQnorm, grid, nbPerms,
				trick, whichPermBf, rngPerm, rngTrick);
    }
  }
  if (verbose == 1)
    cout << endl << flush;
}

void
makePerms (
  map<string, Ftr> & mFtrs,
  const vector<vector<size_t> > & vvSampleIdxPhenos,
  const vector<vector<size_t> > & vvSampleIdxGenos,
  const int & whichStep,
  const bool & needQnorm,
  const vector<vector<double> > & grid,
  const size_t & nbPerms,
  const size_t & seed,
  const int & trick,
  const string & whichPermBf,
  const int & verbose)
{
  if (verbose > 0)
    cout << "get feature-level P-values by permuting phenotypes ..." << endl
	 << "permutation"<< (nbPerms > 1 ? "s=" : "=") << nbPerms
	 << ", seed=" << seed
	 << ", trick=" << trick
	 << endl << flush;
  
  gsl_rng * rngPerm = NULL, * rngTrick = NULL;
  gsl_rng_env_setup();
  rngPerm = gsl_rng_alloc (gsl_rng_default);
  if (rngPerm == NULL)
  {
    cerr << "ERROR: can't allocate memory for the RNG" << endl;
    exit (1);
  }
  if (trick != 0)
  {
    rngTrick = gsl_rng_alloc (gsl_rng_default);
    if (rngTrick == NULL)
    {
      cerr << "ERROR: can't allocate memory for the RNG" << endl;
      exit (1);
    }
  }
  
  if (whichStep == 2 || whichStep == 5)
    makePermsSepOnePop (mFtrs, vvSampleIdxPhenos, vvSampleIdxPhenos,
			needQnorm, nbPerms, seed, trick, rngPerm, rngTrick,
			verbose);
  
  if (whichStep == 4 || whichStep == 5)
    makePermsJointOnePop (mFtrs, vvSampleIdxPhenos, vvSampleIdxPhenos,
			  needQnorm, grid, nbPerms, seed, trick, whichPermBf,
			  rngPerm, rngTrick, verbose);
  
  gsl_rng_free (rngPerm);
  if (trick != 0)
    gsl_rng_free (rngTrick);
}

void
writeResSstats (
  const string & outPrefix,
  const map<string, Ftr> & mFtrs,
  const map<string, Snp> & mSnps,
  const vector<string> & vSubgroups,
  const int & verbose)
{
  if (verbose > 0)
    cout << "write results of summary statistics in each subgroup ..." << endl << flush;
  
  for (size_t s = 0; s < vSubgroups.size(); ++s)
  {
    stringstream ss;
    ss << outPrefix << "_sumstats_" << vSubgroups[s] << ".txt.gz";
    if (verbose > 0)
      cout << "file " << ss.str() << endl << flush;
    ogzstream outStream;
    openFile (ss.str(), outStream);
    
    outStream << "ftr snp maf n betahat sebetahat sigmahat betaPval pve";
    outStream << endl;
    
    for (map<string, Ftr>::const_iterator itF = mFtrs.begin();
	 itF != mFtrs.end(); ++itF)
    {
      const Ftr * ptF = &(itF->second);
      for (vector<ResFtrSnp>::const_iterator itP = ptF->vResFtrSnps.begin();
	   itP != ptF->vResFtrSnps.end(); ++itP)
	outStream << ptF->name
		  << " " << itP->snp
		  << " " << mSnps.find(itP->snp)->second.vMafs[0]
		  << " " << itP->vNs[s]
		  << " " << itP->vBetahats[s]
		  << " " << itP->vSebetahats[s]
		  << " " << itP->vSigmahats[s]
		  << " " << itP->vBetaPvals[s]
		  << " " << itP->vPves[s]
		  << endl;
    }
    
    outStream.close();
  }
}

void
writeResSepPermPval (
  const string & outPrefix,
  const map<string, Ftr> & mFtrs,
  const vector<string> & vSubgroups,
  const int & verbose)
{
  if (verbose > 0)
    cout << "write results of feature-level P-values in each subgroup ..." << endl << flush;
  
  for (size_t s = 0; s < vSubgroups.size(); ++s)
  {
    stringstream ssOutFile;
    ssOutFile << outPrefix << "_permPval_" << vSubgroups[s] << ".txt.gz";
    if (verbose > 0)
      cout << "file " << ssOutFile.str() << endl << flush;
    ogzstream outStream;
    openFile (ssOutFile.str(), outStream);
    
    outStream << "ftr nbSnps permPval nbPerms";
    outStream << endl;
    
    for (map<string, Ftr>::const_iterator itF = mFtrs.begin();
	 itF != mFtrs.end(); ++itF)
    {
      const Ftr * ptF = &(itF->second);
      outStream << ptF->name
		<< " " << ptF->vPtCisSnps.size()
		<< " " << ptF->vPermPvalsSep[s]
		<< " " << ptF->vNbPermsSoFar[s]
		<< endl;
    }
    
    outStream.close();
  }
}

void
writeResAbfsUnweighted (
  const string & outPrefix,
  const map<string, Ftr> & mFtrs,
  const size_t & nbSubgroups,
  const vector<vector<double> > & grid,
  const string & whichBfs,
  const int & verbose)
{
  if (verbose > 0)
    cout << "write results of Bayes Factors, all subgroups jointly ..." << endl << flush;
  
  gsl_combination * comb;
  stringstream ssOutFile, ssConfig;
  ssOutFile << outPrefix << "_abfs_unweighted.txt.gz";
  if (verbose > 0)
    cout << "file " << ssOutFile.str() << endl << flush;
  ogzstream outStream;
  openFile (ssOutFile.str(), outStream);
  
  // write header line
  outStream << "ftr snp config";
  for (size_t i = 0; i < grid.size(); ++i)
    outStream << " ABFgrid" << (i+1);
  outStream << endl;
  
  for (map<string, Ftr>::const_iterator itF = mFtrs.begin();
       itF != mFtrs.end(); ++itF)
  {
    const Ftr * ptF = &(itF->second);
    
    for (vector<ResFtrSnp>::const_iterator itP = ptF->vResFtrSnps.begin();
	 itP != ptF->vResFtrSnps.end(); ++itP)
    {
      outStream << ptF->name
		<< " " << itP->snp
		<< " const";
      for (size_t i = 0; i < grid.size(); ++i)
	outStream << " " << itP->mUnweightedAbfs.find("const")->second[i];
      outStream << endl;
      
      if (whichBfs.compare("const") != 0)
      {
	for (size_t k = 1; k < nbSubgroups; ++k)
	{
	  comb = gsl_combination_calloc (nbSubgroups, k);
	  if (comb == NULL)
	  {
	    cerr << "ERROR: can't allocate memory for the combination" << endl;
	    exit (1);
	  }
	  while (true)
	  {
	    ssConfig.str("");
	    ssConfig << gsl_combination_get (comb, 0) + 1;
	    if (comb->k > 1)
	      for (size_t i = 1; i < k; ++i)
		ssConfig << "-" << gsl_combination_get (comb, i) + 1;
	    outStream << ptF->name
		      << " " << itP->snp
		      << " " << ssConfig.str();
	    for (size_t i = 0; i < grid.size(); ++i)
	      outStream << " " << itP->mUnweightedAbfs.find(ssConfig.str())->second[i];
	    outStream << endl;
	    if (gsl_combination_next (comb) != GSL_SUCCESS)
	      break;
	  }
	  if (whichBfs.compare("subset") == 0)
	    break;
	}
      }
    }
  }
  
  outStream.close();
}

void
writeResAbfsWeighted (
  const string & outPrefix,
  const map<string, Ftr> & mFtrs,
  const size_t & nbSubgroups,
  const string & whichBfs,
  const int & verbose)
{
  if (verbose > 0)
    cout << "write results of Bayes Factors, all subgroups jointly ..." << endl << flush;
  
  gsl_combination * comb;
  stringstream ssOutFile, ssConfig;
  ssOutFile << outPrefix << "_abfs_weighted.txt.gz";
  if (verbose > 0)
    cout << "file " << ssOutFile.str() << endl << flush;
  ogzstream outStream;
  openFile (ssOutFile.str(), outStream);
  
  // write header line
  outStream << "ftr snp nb.subgroups nb.samples abf.const abf.const.fix abf.const.maxh";
  if (whichBfs.compare("const") != 0)
  {
    for (size_t k = 1; k < nbSubgroups; ++k)
    {
      comb = gsl_combination_calloc (nbSubgroups, k);
      if (comb == NULL)
      {
	cerr << "ERROR: can't allocate memory for the combination" << endl;
	exit (1);
      }
      while (true)
      {
	outStream << " abf." << gsl_combination_get (comb, 0) + 1;
	if (comb->k > 1)
	  for (size_t i = 1; i < k; ++i)
	    outStream << "-" << gsl_combination_get (comb, i) + 1;
	if (gsl_combination_next (comb) != GSL_SUCCESS)
	  break;
      }
      if (whichBfs.compare("subset") == 0)
	break;
    }
  }
  outStream << endl;
  
  for (map<string, Ftr>::const_iterator itF = mFtrs.begin();
       itF != mFtrs.end(); ++itF)
  {
    const Ftr * ptF = &(itF->second);
    for (vector<ResFtrSnp>::const_iterator itP = ptF->vResFtrSnps.begin();
	 itP != ptF->vResFtrSnps.end(); ++itP)
    {
      outStream << ptF->name
		<< " " << itP->snp
		<< " " << count_if (itP->vNs.begin(), itP->vNs.end(), isNonZero)
		<< " " << accumulate (itP->vNs.begin(), itP->vNs.end(), 0)
		<< " " << itP->mWeightedAbfs.find("const")->second
		<< " " << itP->mWeightedAbfs.find("const-fix")->second
		<< " " << itP->mWeightedAbfs.find("const-maxh")->second;
      if (whichBfs.compare("const") != 0)
      {
	for (size_t k = 1; k < nbSubgroups; ++k)
	{
	  comb = gsl_combination_calloc (nbSubgroups, k);
	  if (comb == NULL)
	  {
	    cerr << "ERROR: can't allocate memory for the combination" << endl;
	    exit (1);
	  }
	  while (true)
	  {
	    ssConfig.str("");
	    ssConfig << gsl_combination_get (comb, 0) + 1;
	    if (comb->k > 1)
	      for (size_t i = 1; i < k; ++i)
		ssConfig << "-" << gsl_combination_get (comb, i) + 1;
	    outStream << " " << itP->mWeightedAbfs.find(ssConfig.str())->second;
	    if (gsl_combination_next (comb) != GSL_SUCCESS)
	      break;
	  }
	  if (whichBfs.compare("subset") == 0)
	    break;
	}
      }
      outStream << endl;
    }
  }
  
  outStream.close();
}

void
writeResJointPermPval (
  const string & outPrefix,
  const map<string, Ftr> & mFtrs,
  const int & verbose)
{
  if (verbose > 0)
    cout << "write results of feature-level P-values, all subgroups jointly ..." << endl << flush;
  
  stringstream ssOutFile;
  ssOutFile << outPrefix << "_jointPermPvals.txt.gz";
  if (verbose > 0)
    cout << "file " << ssOutFile.str() << endl << flush;
  ogzstream outStream;
  openFile (ssOutFile.str(), outStream);
  
  outStream << "ftr nbSnps jointPermPval nbPerms maxL10TrueAbf" << endl;
  
  for (map<string, Ftr>::const_iterator itF = mFtrs.begin();
       itF != mFtrs.end(); ++itF)
    outStream << itF->second.name
	      << " " << itF->second.vPtCisSnps.size()
	      << " " << itF->second.jointPermPval
	      << " " << itF->second.nbPermsSoFar
	      << " " << itF->second.maxL10TrueAbf
	      << endl;
  
  outStream.close();
}

void
writeRes (
  const string & outPrefix,
  const map<string, Ftr> & mFtrs,
  const map<string, Snp> & mSnps,
  const vector<string> & vSubgroups,
  const int & whichStep,
  const vector<vector<double> > & grid,
  const string & whichBfs,
  const int & verbose)
{
  writeResSstats (outPrefix, mFtrs, mSnps, vSubgroups, verbose);
  
  if (whichStep == 2 || whichStep == 5)
    writeResSepPermPval (outPrefix, mFtrs, vSubgroups, verbose);
  
  if (whichStep == 3 || whichStep == 4 || whichStep == 5)
  {
    writeResAbfsUnweighted (outPrefix, mFtrs, vSubgroups.size(), grid,
			    whichBfs, verbose);
    writeResAbfsWeighted (outPrefix, mFtrs, vSubgroups.size(), whichBfs,
			  verbose);
  }
  
  if (whichStep == 4 || whichStep == 5)
    writeResJointPermPval (outPrefix, mFtrs, verbose);
}

void
run (
  const string & genoPathsFile,
  const string & phenoPathsFile,
  const string & ftrCoordsFile,
  const string & anchor,
  const size_t & lenCis,
  const string & outPrefix,
  const int & whichStep,
  const bool & needQnorm,
  const string & gridFile,
  const string & whichBfs,
  const size_t & nbPerms,
  const size_t & seed,
  const int & trick,
  const string & whichPermBf,
  const string & ftrsToKeepFile,
  const string & snpsToKeepFile,
  const int & verbose)
{
  vector<string> vFtrsToKeep = loadOneColumnFile (ftrsToKeepFile, verbose);
  vector<string> vSnpsToKeep = loadOneColumnFile (snpsToKeepFile, verbose);
  vector<vector<double> > grid = loadGrid (gridFile, verbose);
  
  map<string, string> mGenoPaths, mPhenoPaths;
  vector<string> vSubgroups;
  loadListsGenoAndPheno (genoPathsFile, phenoPathsFile, mGenoPaths,
			 mPhenoPaths, vSubgroups, verbose);
  
  vector<string> vSamples;
  vector<vector<size_t> > vvSampleIdxGenos, vvSampleIdxPhenos;
  loadSamples (mGenoPaths, mPhenoPaths, vSubgroups, vSamples,
	        vvSampleIdxGenos, vvSampleIdxPhenos, verbose);
  
  map<string, Ftr> mFtrs;
  map<string, vector<Ftr*> > mChr2VecPtFtrs;
  loadPhenos (mPhenoPaths, vSubgroups, vFtrsToKeep, mFtrs, verbose);
  loadFtrInfo (ftrCoordsFile, mFtrs, mChr2VecPtFtrs, verbose);
  
  map<string, Snp> mSnps;
  map<string, vector<Snp*> > mChr2VecPtSnps;
  loadGenosAndSnpInfo (mGenoPaths, vSubgroups, vSnpsToKeep, mSnps,
		       mChr2VecPtSnps, verbose);
  
  inferAssos (mFtrs, mChr2VecPtFtrs, mSnps, mChr2VecPtSnps, vvSampleIdxPhenos,
	      vvSampleIdxGenos, anchor, lenCis, whichStep, needQnorm, grid,
	      whichBfs, verbose);
  if (whichStep == 2 || whichStep == 4 || whichStep == 5)
    makePerms (mFtrs, vvSampleIdxPhenos, vvSampleIdxGenos, whichStep, 
	       needQnorm, grid, nbPerms, seed, trick, whichPermBf, verbose);
  
  writeRes (outPrefix, mFtrs, mSnps, vSubgroups, whichStep, grid, whichBfs,
	    verbose);
}

#ifdef EQTLBMA_MAIN

int main (int argc, char ** argv)
{
  int verbose = 1, whichStep = 0, trick = 0;
  size_t lenCis = 100000, nbPerms = 0, seed = string::npos;
  bool needQnorm = false;
  string genoPathsFile, phenoPathsFile, ftrCoordsFile, anchor = "FSS",
    outPrefix, gridFile, whichBfs = "const", whichPermBf = "const",
    ftrsToKeepFile, snpsToKeepFile;
  
  parseArgs (argc, argv, genoPathsFile, phenoPathsFile, ftrCoordsFile,
	     anchor, lenCis, outPrefix, whichStep, needQnorm, gridFile,
	     whichBfs, nbPerms, seed, trick, whichPermBf, ftrsToKeepFile,
	     snpsToKeepFile, verbose);
  
  time_t startRawTime, endRawTime;
  if (verbose > 0)
  {
    time (&startRawTime);
    cout << "START " << argv[0] << " (" << time2string (startRawTime) << ")"
	 << endl;
  }
  
  run (genoPathsFile, phenoPathsFile, ftrCoordsFile, anchor, lenCis,
       outPrefix, whichStep, needQnorm, gridFile, whichBfs,
       nbPerms, seed, trick, whichPermBf, ftrsToKeepFile, snpsToKeepFile,
       verbose);
  
  if (verbose > 0)
  {
    time (&endRawTime);
    cout << "END " << argv[0] << " (" << time2string (endRawTime) << ")"
	 << endl
	 << "elapsed -> " << elapsedTime(startRawTime, endRawTime)
	 << endl
	 << "max.mem -> " << getMaxMemUsedByProcess () << " kB"
	 << endl;
  }
  
  return EXIT_SUCCESS;
}

#endif
