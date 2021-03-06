#pragma once
#include <string>
#include <vector>
#include <stdint.h>

//! A Gene annotation
struct GeneAnnotation
{
  std::string tag;
  std::string type;
  std::string name;
  bool strand;
  uint64_t startPos;
  uint64_t stopPos;
  bool gene;
};

inline bool operator<(const GeneAnnotation&A, const GeneAnnotation& B)
{
  return A.startPos < B.startPos;
}

//! Provides GeneAnnotation objects as read from a GFF3 file
class GeneAnnotationReader
{
public:
  GeneAnnotationReader(const std::string& fname); //!< Parse GFF3 from fname
  std::vector<GeneAnnotation> lookup(uint64_t pos); //!< Get all annotations for pos
  uint64_t size() const { return d_gas.size(); } //!< Number of annotations known

private:
  typedef std::vector<GeneAnnotation> gas_t;
  void parseGenBank(const std::string& fname);
  gas_t d_gas;
};

std::vector<GeneAnnotation> parseGenBankString(const std::string& bank);
