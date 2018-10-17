// system includes
#include <dirent.h>
#include <iostream>
#include <map>
#include <string>
#include <sys/types.h>

// ROOT includes
#include "TFile.h"
#include "TH2F.h"
#include "TTree.h"

// user includes
#include "../include/CLParser.h"

// class to hold the histograms until I'm ready to write them
class histHolder {
public:
  histHolder();
  ~histHolder();
  void writeHistos();
  void initVectors(std::string);

  TH2F *qcd_0jet_OS, *qcd_boosted_OS, *qcd_vbf_OS,
       *qcd_0jet_SS, *qcd_boosted_SS, *qcd_vbf_SS,
       *qcd_0jet   , *qcd_boosted   , *qcd_vbf;
  TFile *fout;
  std::map<std::string, std::vector<TH2F *>> hists;

  // binning
  std::vector<Float_t> bins_l2, bins_hpt, bins_mjj, bins_lpt, bins_msv1, bins_msv2;
};

void read_directory(const std::string &name, std::vector<std::string> &v);
void fillQCD(TH2F *, std::string, double, double, double);

int main(int argc, char *argv[]) {
  // get CLI arguments
  CLParser *parser = new CLParser(argc, argv);
  std::string dir = parser->Option("-d");

  // get input file directory
  if (dir.empty()) {
    std::cerr << "You must give an input directory" << std::endl;
    return -1;
  }

  // initialize histogram holder
  histHolder *hists = new histHolder();

  // read all files from input directory
  std::vector<std::string> files;
  read_directory(dir, files);

  for (auto ifile : files) {
    auto fin = new TFile((dir + "/" + ifile).c_str(), "read");
    auto tree = (TTree *)fin->Get("etau_tree");
    std::string name = ifile.substr(0, ifile.find(".")).c_str();

    hists->initVectors(name);

    // I hate doing it like this, but when I move the SetBranchAddres I see unexpected behavior
    Int_t cat_inclusive, cat_0jet, cat_boosted, cat_vbf, cat_antiiso, cat_antiiso_0jet, cat_antiiso_boosted, cat_antiiso_vbf, cat_qcd, cat_qcd_0jet, cat_qcd_boosted, cat_qcd_vbf;
    Float_t eq, tq, hpt, l2decay, vis_mass, mjj, m_sv, weight;
    tree->SetBranchAddress("el_charge", &eq);
    tree->SetBranchAddress("t1_charge", &tq);
    tree->SetBranchAddress("higgs_pT", &hpt);
    tree->SetBranchAddress("l2decay", &l2decay);
    tree->SetBranchAddress("vis_mass", &vis_mass);
    tree->SetBranchAddress("mjj", &mjj);
    tree->SetBranchAddress("m_sv", &m_sv);
    tree->SetBranchAddress("evtwt", &weight);
    tree->SetBranchAddress("cat_inclusive", &cat_inclusive);
    tree->SetBranchAddress("cat_vbf", &cat_vbf);
    tree->SetBranchAddress("cat_boosted", &cat_boosted);
    tree->SetBranchAddress("cat_0jet", &cat_0jet);
    tree->SetBranchAddress("cat_antiiso", &cat_antiiso);
    tree->SetBranchAddress("cat_antiiso_0jet", &cat_antiiso_0jet);
    tree->SetBranchAddress("cat_antiiso_boosted", &cat_antiiso_boosted);
    tree->SetBranchAddress("cat_antiiso_vbf", &cat_antiiso_vbf);
    tree->SetBranchAddress("cat_qcd", &cat_qcd);
    tree->SetBranchAddress("cat_qcd_0jet", &cat_qcd_0jet);
    tree->SetBranchAddress("cat_qcd_boosted", &cat_qcd_boosted);
    tree->SetBranchAddress("cat_qcd_vbf", &cat_qcd_vbf);

    for (auto i = 0; i < tree->GetEntries(); i++) {
      tree->GetEntry(i);

      if (eq + tq == 0) {
        // output histograms for the template
        if (cat_0jet > 0) {
          hists->hists.at("et_0jet").back()->Fill(l2decay, vis_mass, weight);
        }
        if (cat_boosted > 0) {
          hists->hists.at("et_boosted").back()->Fill(hpt, m_sv, weight);
        }
        if (cat_vbf > 0 && hpt > 50) {
          hists->hists.at("et_vbf").back()->Fill(mjj, m_sv, weight);
        }
      } else {
        // get QCD shape from SS loose iso region
        if (cat_qcd_0jet > 0) {
          fillQCD(hists->qcd_0jet, name, l2decay, vis_mass, weight);
        }
        if (cat_qcd_boosted > 0) {
          fillQCD(hists->qcd_boosted, name, hpt, m_sv, weight);
        }
        if (cat_qcd_vbf > 0 && hpt > 50) {
          fillQCD(hists->qcd_vbf, name, mjj, m_sv, weight);
        }

        // get SS in signal region for loose region normalization
        if (cat_0jet > 0) {
          fillQCD(hists->qcd_0jet_SS, name, l2decay, vis_mass, weight);
        }
        if (cat_boosted > 0) {
          fillQCD(hists->qcd_boosted_SS, name, hpt, m_sv, weight);
        }
        if (cat_vbf > 0 && hpt > 50) {
          fillQCD(hists->qcd_vbf_SS, name, mjj, m_sv, weight);
        }
      }
    }
  }
  // write the output histograms
  hists->writeHistos();
  delete hists;
  delete parser;
}

// read all *.root files in the given directory and put them in the provided vector
void read_directory(const std::string &name, std::vector<std::string> &v) {
  DIR *dirp = opendir(name.c_str());
  struct dirent *dp;
  while ((dp = readdir(dirp)) != 0) {
    if (static_cast<std::string>(dp->d_name).find("root") != std::string::npos) {
      v.push_back(dp->d_name);
    }
  }
  closedir(dirp);
}

// Fill histogram with positive weight for Data and negative weight for BKG. Equivalent to
// doing data-bkg
void fillQCD(TH2F *hist, std::string name, double var1, double var2, double weight) {
  if (name.find("Data") != std::string::npos) {
    hist->Fill(var1, var2, weight);
  } else if (name == "embed" || name == "ZL" || name == "ZJ" || name == "TTT" || name == "TTJ" || name == "W" || name == "VV") {
    hist->Fill(var1, var2, -1 * weight);
  }
}

// histHolder contructor to create the output file, the qcd histograms with the correct binning
// and the map from categories to vectors of TH1F*'s. Each TH1F* in the vector corresponds to
// one file that is being put into that categories directory in the output tempalte
histHolder::histHolder() : 
    hists{
        {"et_0jet", std::vector<TH2F *>()},
        {"et_boosted", std::vector<TH2F *>()},
        {"et_vbf", std::vector<TH2F *>()},
    },
    fout(new TFile("Output/templates/template_final.root", "recreate")),
    // x-axis
    bins_l2 {0, 1, 10, 11},
    bins_hpt {0, 100, 150, 200, 250, 300, 5000},
    bins_mjj {300, 700, 1100, 1500, 10000},

    // y-axis
    bins_lpt {0, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 400},
    bins_msv1 {0, 80, 90, 100, 110, 120, 130, 140, 150, 160, 300},
    bins_msv2 {0, 95, 115, 135, 155, 400}
{
    for (auto it = hists.begin(); it != hists.end(); it++) {
        fout->cd();
        fout->mkdir((it->first).c_str());
        fout->cd();
    }
    qcd_0jet       = new TH2F("qcd_0jet"      , "qcd_0jet"      , bins_l2.size()  - 1, &bins_l2[0] , bins_lpt.size()  - 1, &bins_lpt[0] );
    qcd_0jet_OS    = new TH2F("qcd_0jet_OS"   , "qcd_0jet_OS"   , bins_l2.size()  - 1, &bins_l2[0] , bins_lpt.size()  - 1, &bins_lpt[0] );
    qcd_0jet_SS    = new TH2F("qcd_0jet_SS"   , "qcd_0jet_SS"   , bins_l2.size()  - 1, &bins_l2[0] , bins_lpt.size()  - 1, &bins_lpt[0] );
    qcd_boosted    = new TH2F("qcd_boosted"   , "qcd_boosted"   , bins_hpt.size() - 1, &bins_hpt[0], bins_msv1.size() - 1, &bins_msv1[0]);
    qcd_boosted_OS = new TH2F("qcd_boosted_OS", "qcd_boosted_OS", bins_hpt.size() - 1, &bins_hpt[0], bins_msv1.size() - 1, &bins_msv1[0]);
    qcd_boosted_SS = new TH2F("qcd_boosted_SS", "qcd_boosted_SS", bins_hpt.size() - 1, &bins_hpt[0], bins_msv1.size() - 1, &bins_msv1[0]);
    qcd_vbf        = new TH2F("qcd_vbf"       , "qcd_vbf"       , bins_mjj.size() - 1, &bins_mjj[0], bins_msv2.size() - 1, &bins_msv2[0]);
    qcd_vbf_SS     = new TH2F("qcd_vbf_SS"    , "qcd_vbf_SS"    , bins_mjj.size() - 1, &bins_mjj[0], bins_msv2.size() - 1, &bins_msv2[0]);
    qcd_vbf_OS     = new TH2F("qcd_vbf_OS"    , "qcd_vbf_OS"    , bins_mjj.size() - 1, &bins_mjj[0], bins_msv2.size() - 1, &bins_msv2[0]);
}

histHolder::~histHolder() {}

// change to the correct output directory then create a new TH1F that will be filled for the current input file
void histHolder::initVectors(std::string name) {
    for (auto key : hists) {
        fout->cd(key.first.c_str());
        if (name.find("Data") != std::string::npos) {
            name = "data_obs";
        }
        if (key.first == "et_0jet") {
            hists.at(key.first.c_str()).push_back(new TH2F(name.c_str(), name.c_str(), bins_l2.size()  - 1, &bins_l2[0] , bins_lpt.size()  - 1, &bins_lpt[0] ));
        } else if (key.first == "et_boosted") {                                                                          
            hists.at(key.first.c_str()).push_back(new TH2F(name.c_str(), name.c_str(), bins_hpt.size() - 1, &bins_hpt[0], bins_msv1.size() - 1, &bins_msv1[0]));
        } else if (key.first == "et_vbf") {                                                                              
            hists.at(key.first.c_str()).push_back(new TH2F(name.c_str(), name.c_str(), bins_mjj.size() - 1, &bins_mjj[0], bins_msv2.size() - 1, &bins_msv2[0]));
        }
    }
}

// write output histograms including the QCD histograms after scaling by OS/SS ratio
void histHolder::writeHistos() {
  for (auto cat : hists) {
    fout->cd(cat.first.c_str());
    for (auto hist : cat.second) {
      hist->Write();
    }
  }

  fout->cd("et_0jet");
  qcd_0jet->SetName("QCD");
  qcd_0jet->Scale(1.0 * qcd_0jet_SS->Integral() / qcd_0jet->Integral());
  for (auto i = 0; i < qcd_0jet->GetNbinsX(); i++) {
    if (qcd_0jet->GetBinContent(i) < 0) {
      qcd_0jet->SetBinContent(i, 0);
    }
  }
  qcd_0jet->Write();

  fout->cd("et_boosted");
  qcd_boosted->SetName("QCD");
  qcd_boosted->Scale(1.28 * qcd_boosted_SS->Integral() / qcd_boosted->Integral());
  for (auto i = 0; i < qcd_boosted->GetNbinsX(); i++) {
    if (qcd_boosted->GetBinContent(i) < 0) {
      qcd_boosted->SetBinContent(i, 0);
    }
  }
  qcd_boosted->Write();

  fout->cd("et_vbf");
  qcd_vbf->SetName("QCD");
  qcd_vbf->Scale(1.0 * qcd_vbf_SS->Integral() / qcd_vbf->Integral());
  for (auto i = 0; i < qcd_vbf->GetNbinsX(); i++) {
    if (qcd_vbf->GetBinContent(i) < 0) {
      qcd_vbf->SetBinContent(i, 0);
    }
  }
  qcd_vbf->Write();
  fout->Close();
}
