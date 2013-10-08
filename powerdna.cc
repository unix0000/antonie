#include <stdio.h>
#include <iostream>
#include <stdint.h>
#include <map>
#include <vector>
#include <string>
#include <string.h>
#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/progress.hpp>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "hash.h"
}
using namespace std;

char* sfgets(char* p, int num, FILE* fp)
{
  char *ret = fgets(p, num, fp);
  if(!ret) 
    throw runtime_error("Unexpected EOF");
  return ret;
}
 
class ReferenceGenome
{
public:
  ReferenceGenome(FILE* fp);
  uint64_t size() const {
    return d_genome.size();
  }
  
  uint64_t getFragmentPos(const std::string& str) 
  {
    if(str.length() != d_indexlength)
      throw runtime_error("Attempting to find fragment of length we've not indexed for");
      
    uint32_t hashval = hash(str.c_str(), str.length(), 0);
    index_t::const_iterator iter = d_index.find(hashval);
    if(iter == d_index.end())
      return string::npos;

    const char* pos=0;
    BOOST_FOREACH(uint64_t off, iter->second) {
      pos = strstr(d_genome.c_str() + off, str.c_str());
      if(pos)
	cover(pos - d_genome.c_str(), str.size());
    }
    if(pos)
      return pos - d_genome.c_str();
    return string::npos;
  }

  void cover(uint64_t pos, unsigned int length) {
    while(length-- > 0) 
      d_coverage[pos++]++;
  }

  void printCoverage();
  void index(int length);

private:
  unsigned int d_indexlength;
  string d_genome;
  vector<uint16_t> d_coverage;
  typedef map<uint32_t, vector<uint64_t> > index_t;
  index_t d_index;
};

void chomp(char* line)
{
  char *p;
  p = strchr(line, '\r');
  if(p)*p=0;
  p = strchr(line, '\n');
  if(p)*p=0;
}

ReferenceGenome::ReferenceGenome(FILE *fp)
{
  char line[256]="";

  sfgets(line, sizeof(line), fp);
  if(line[0] != '>') 
    throw runtime_error("Input not FASTA");

  while(fgets(line, sizeof(line), fp)) {
    chomp(line);
    d_genome.append(line);
  }
  d_coverage.resize(d_genome.size());
  d_indexlength=0;
}

void ReferenceGenome::index(int length)
{
  d_indexlength=length;
  cerr<<"Indexing "<<d_genome.length()<<" nucleotides for length "<<length<<endl;
  boost::progress_display show_progress( d_genome.length(), cerr);

  for(string::size_type pos = 0 ; pos < d_genome.length() - length; ++pos) {
    uint32_t hashval = hash(d_genome.c_str() + pos, length, 0);
    d_index[hashval].push_back(pos);
    ++show_progress;
  }
  cerr<<endl;
  cerr<<"Average hash fill: "<<1.0*d_genome.length()/d_index.size()<<endl;
}

struct FastQFragment
{
  string d_nucleotides;
  void reverse();
};

void FastQFragment::reverse()
{
  std::reverse(d_nucleotides.begin(), d_nucleotides.end());
  for(string::iterator iter = d_nucleotides.begin(); iter != d_nucleotides.end(); ++iter) {
    if(*iter == 'C')
      *iter = 'G';
    else if(*iter == 'G')
      *iter = 'C';
    else if(*iter == 'A')
      *iter = 'T';
    else if(*iter == 'T')
      *iter = 'A';
  }
}


unsigned int getFragment(FILE* fastq, FastQFragment* fq)
{
  uint64_t pos = ftell(fastq);
  char line[256]="";
  if(!fgets(line, sizeof(line), fastq)) 
    return 0;
  if(line[0] != '@')
    throw runtime_error("Input not FASTQ");

  sfgets(line, sizeof(line), fastq);
  chomp(line);
  fq->d_nucleotides=line;
  sfgets(line, sizeof(line), fastq);
  sfgets(line, sizeof(line), fastq);
  return ftell(fastq) - pos;

}

void ReferenceGenome::printCoverage()
{
  uint64_t totCoverage=0, noCoverage=0;
  unsigned int cov;

  vector<unsigned int> nulls;
  nulls.resize(2000);
  unsigned int binwidth = d_coverage.size()/2000;

  bool wasNul=true;
  string::size_type prevNulpos=0;

  for(string::size_type pos = 0; pos < d_coverage.size(); ++pos) {
    cov = d_coverage[pos];
    totCoverage += cov;
//    printf("%ld %d\n", pos, cov);
    if(!cov) {
      noCoverage++;
      nulls[pos / binwidth]++;
    }
    
    if(cov && wasNul) {
      cout<<"SNP: "<<prevNulpos << " - " << pos<< " (len = "<< pos-prevNulpos<<")"<<endl;
      if(prevNulpos > 50 && pos + 50 < d_genome.length())
        cout<<"\t"<<d_genome.substr(prevNulpos-50, 50) << " | "<< d_genome.substr(prevNulpos, pos-prevNulpos)<< " | " << d_genome.substr(pos, 50)<<endl;
      wasNul=false;
    }
    else if(!cov && !wasNul) {
      wasNul=true;
      prevNulpos = pos;
    }

  }
  cerr<<"Average depth: "<<totCoverage/d_coverage.size()<<endl;
  cerr<<"No coverage: "<<noCoverage*100.0/d_coverage.size()<<"%"<<endl;
  
  for(unsigned int n = 0; n < 2000; ++n) 
    cout<<n*binwidth<<"\t"<<nulls[n]<<endl;
  
}

uint64_t filesize(const char* name)
{
  struct stat buf;
  if(!stat(name, &buf)) {
    return buf.st_size;
  }
  return 0;
}

int main(int argc, char** argv)
{
  if(argc!=3) {
    fprintf(stderr, "Syntax: powerdna fastqfile fastafile\n");
    exit(EXIT_FAILURE);
  }
  FILE* fastq = fopen(argv[1], "r");
  FILE* fasta = fopen(argv[2], "r");

  ReferenceGenome rg(fasta);

  //  cout<<"Reference genome has "<<rg.size()<<" nucleotides"<<endl;

  FastQFragment fq;
  
  unsigned int bytes=0;
  bytes=getFragment(fastq, &fq); // get a fragment and index on it
  rg.index(fq.d_nucleotides.size());

  uint64_t pos;
  uint64_t withAny=0, found=0, notFound=0, total=0, reverseFound=0;
  boost::progress_display show_progress( filesize(argv[1]), cerr);

  do { 
    show_progress += bytes;
    total++;
    if(fq.d_nucleotides.find('N') != string::npos) {
      withAny++;
      continue;
    }
    pos = rg.getFragmentPos(fq.d_nucleotides);
    if(pos == string::npos) {
      fq.reverse();
      pos = rg.getFragmentPos(fq.d_nucleotides);
      if(pos != string::npos)
	reverseFound++;
      else
	notFound++;
    }
    else {
      found++;
    }
  } while((bytes=getFragment(fastq, &fq)));
  cerr<<"Total fragments: "<<total<<endl;
  cerr<<"Found: "<<found<<endl;
  cerr<<"Reverse found: "<<reverseFound<<endl;
  cerr<<"Not found: "<< notFound<<endl;
  cerr<<"With N: "<<withAny++<<endl;
  
  rg.printCoverage();
}
