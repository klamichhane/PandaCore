#include "../interface/HistogramDrawer.h"
#include "algorithm"
#include <math.h>
#include "THStack.h"
#include "TGraphErrors.h"

HistogramDrawer::HistogramDrawer(double x, double y):
  CanvasDrawer(x,y) {
    // done
  }

HistogramDrawer::~HistogramDrawer() {
  if (fileIsOwned)
    delete centralFile;
}

void HistogramDrawer::AddAdditional(TObject *o, TString opt, TString aname) {
  ObjWrapper w;
  w.o = o;
  w.opt = opt;
  w.label = aname;
}

void HistogramDrawer::AddHistogram(TH1F *h, TString label, ProcessType pt, int cc, TString opt) {
  if (h!=NULL) {
    h->SetStats(0);
    h->SetTitle("");

    HistWrapper w;
    w.h = h;
    w.label = label;
    if (pt==nProcesses) 
      w.pt = (ProcessType)(internalHists.size()+2);
    else
      w.pt = pt;
    if (cc<0) 
      w.cc = PlotColors[w.pt];
    else
      w.cc = cc;
    if (opt=="")
      w.opt = drawOption;
    else
      w.opt = opt;

    internalHists.push_back(w);
  }
}

void HistogramDrawer::AddHistogram(TString hname, TString label, ProcessType pt, TString fname) {
  TFile *tmpFile;
  if (fname!="") 
    tmpFile = TFile::Open(fname);
  else 
    tmpFile = centralFile;

  TH1F *h = (TH1F*)tmpFile->Get(hname);
  AddHistogram(h,label,pt);
}

void HistogramDrawer::SetInputFile(TFile *f) {
  centralFile = f;
}

void HistogramDrawer::SetInputFile(TString fname) {
  fileIsOwned = true;
  centralFile = TFile::Open(fname);
}

void HistogramDrawer::Reset() {
  internalHists.clear();
  internalAdds.clear();
  plotLabels.clear();
  if (legend!=NULL)
    legend->Clear();
  if (c!=NULL)
    c->Clear();
}

void HistogramDrawer::Draw(TString outDir, TString baseName) {
  if (c==NULL)
    c = new TCanvas();

//  c->Clear();

  unsigned int nH = internalHists.size();
  if (nH==0) {
    CanvasDrawer::Draw(outDir,baseName);
    return;
  }
  int nBins = internalHists[0].h->GetNbinsX();
  c->cd();
  TPad *pad1=0, *pad2=0;
  float stackIntegral=0;
  THStack *hs=0;
  TH1F *hData=0;
  TH1F *hSignal[4] = {0,0,0,0};
  TH1F *hRatio=0;
  TH1F *hZero=0;
  TH1F *hSum=0;
  TH1F *hRatioError = 0;
  std::vector<HistWrapper> hOthers;

  c->SetLogy(doLogy&&(!doRatio));
  
  if (doStack)
    hs = new THStack("h","");
  
  for (unsigned int iH=0; iH!=nH; ++iH) {
    HistWrapper w = internalHists[iH];
    TH1F *h = w.h;
    ProcessType pt = w.pt;
    if (doStack && pt>kSignal3) {
      h->SetLineColor(1);
    } else {
      h->SetLineColor(w.cc);
      h->SetLineWidth(3);
    }
    TString legOption = "L";
    if (w.opt.Contains("elp"))
      legOption = "ELP";
    if (pt!=kData && (pt==kData || pt>kSignal3 || doStackSignal)) {
      if (doStack) {
        // if it's stackable
        h->SetFillColor(w.cc);
        hs->Add(h);
        legOption = "F";
        stackIntegral += h->Integral();
      } else {
        if (doSetNormFactor) 
          h->Scale(1./h->Integral());
      }
      hOthers.push_back(w);
    } else if (pt==kData) {
      // if it's data
      legOption = "ELP";
      hData = h;
      hData->SetMarkerColor(PlotColors[pt]);
      hData->SetMarkerStyle(20);
      if (doSetNormFactor) {
        for (int iB=0; iB!=nBins; ++iB) {
          hData->SetBinError(iB,hData->GetBinError(iB)/hData->Integral());
        }
        hData->Scale(1./hData->Integral());
      }
    } else if (pt<=kSignal3 && pt!=kData) {
      // if it's signal and we're not stacking signal
      hSignal[pt-1] = h;
      hSignal[pt-1]->SetLineWidth(5);
      hSignal[pt-1]->SetFillStyle(0);
      if (doSetNormFactor)
        hSignal[pt-1]->Scale(1./hSignal[pt-1]->Integral());
    } 
    if (legend && w.label!="") 
      legend->AddEntry(h,w.label,legOption);
  }


  // scale stacked histograms and calculate stacked error bands
  if (doStack) {
    if (doSetNormFactor) {
      for (HistWrapper ww : hOthers) 
        ww.h->Scale(1./stackIntegral);
    }
    if (doDrawMCErrors) {
      std::vector<float> vals,errs;
      hSum = (TH1F*) (hOthers[0].h->Clone("hsum"));
      hSum->SetFillColorAlpha(kBlack,0.99);
      hSum->SetLineWidth(0);
      hSum->SetFillStyle(3004);
      for (int iB=0; iB!=nBins; ++iB) {
        hSum->SetBinContent(iB+1,0); 
        vals.push_back(0);
        errs.push_back(0);
      }
      
      for (HistWrapper w : hOthers) {
        TH1F *hh = w.h;
        for (int iB=0; iB!=nBins; ++iB) {
          vals[iB] += hh->GetBinContent(iB+1);
          errs[iB] += std::pow(hh->GetBinError(iB+1),2);
        }
      }
      
      for (int iB=0; iB!=nBins; ++iB) {
        hSum->SetBinContent(iB+1,vals[iB]);
        hSum->SetBinError(iB+1,std::sqrt(errs[iB]));
      }
    }
  }

  // figure out min and max 
  double maxY=0, minY=9999;
  if (doStack) {
    if (doLogy) {
      double sumMin=0;
      for (HistWrapper w : hOthers) {
        double tmp_min=9999;
        tmp_min = w.h->GetMinimum();
        if (tmp_min<9999)
          sumMin += tmp_min;
      }
      minY = std::min(minY,sumMin);
    }
    maxY = hs->GetMaximum();        
  } else {
    for (HistWrapper w : hOthers) {
      TH1F *hh = w.h;
      maxY = std::max(maxY,hh->GetMaximum());
      if (doLogy)
        minY = std::min(minY,hh->GetMinimum());
    }
  }
  for (unsigned int iS=0; iS!=4; ++iS) {
    if (!doStackSignal && hSignal[iS]!=NULL) {
      maxY = std::max(hSignal[iS]->GetMaximum(),maxY);
      minY = std::min(hSignal[iS]->GetMinimum(),minY);
    }
  }
  if (hData!=NULL)
    maxY = std::max(maxY,hData->GetMaximum());
  if (doLogy) {
    maxY *= maxScale;
    minY *= 0.5;
    if (absMin!=-999)
      minY = std::max(minY,absMin);
    else
      minY = std::max(minY,0.001);
  } else {
    maxY *= maxScale;
    if (absMin!=-999)
      minY = absMin;
    else
      minY=0;
  }

  // set up canvases
  c->cd();
  if (doRatio) {
    pad1 = new TPad("pad1", "pad1", 0, 0.3, 1, 1.0);
    if (doLogy)
      pad1->SetLogy();
    pad1->SetBottomMargin(0);
    pad1->Draw();
    pad1->cd();
  }
  TString xlabel = internalHists[0].h->GetXaxis()->GetTitle();
  TString ylabel = internalHists[0].h->GetYaxis()->GetTitle();
  for (HistWrapper w : internalHists) {
    w.h->GetXaxis()->SetTitle("");
  }

  // figure out what to do with the first histogram
  TH1F *firstHist=0;
  if (!doStack) {
    if (hOthers.size()>0) 
      firstHist = hOthers[0].h;
    else if (hData)
      firstHist = hData;
    else {
      for (unsigned int iS=0; iS!=4; ++iS) {
        if (hSignal[iS]!=NULL) {
          firstHist = hSignal[iS];
          break;
        }
      }
    }
  }
  if (firstHist) {
    firstHist->SetMinimum(minY);
    firstHist->SetMaximum(maxY);
    if (firstHist==hData)
      firstHist->Draw("elp");
    else if (firstHist==hOthers[0].h)
      firstHist->Draw(hOthers[0].opt);
    else
      firstHist->Draw(drawOption);
    if (!doRatio) {
      firstHist->GetXaxis()->SetTitle(xlabel);
    } else {
      firstHist->GetXaxis()->SetTitle("");
    }
    firstHist->GetYaxis()->SetTitle(ylabel);
    firstHist->GetYaxis()->SetTitleOffset(1.5);
  }

  // plot everything else
  if (doStack) {
    hs->SetMinimum(minY); 
    hs->SetMaximum(maxY);
    hs->Draw(drawOption);
    if (!doRatio) {
      hs->GetXaxis()->SetTitle(xlabel);
    } else {
      hs->GetXaxis()->SetTitle("");
    }
    hs->GetYaxis()->SetTitle(ylabel);
    hs->GetYaxis()->SetTitleOffset(1.5);
    if (doDrawMCErrors)
      hSum->Draw("e2 same");
  } else {
    for (HistWrapper w : hOthers) {
      TH1F *hh = w.h;
      if (hh==firstHist)
        continue;
      hh->Draw(w.opt+" same");
    }
  }
  for (unsigned int iS=0; iS!=4; ++iS) {
    if (hSignal[iS]!=NULL && firstHist!=hSignal[iS]) {
      hSignal[iS]->Draw(drawOption+" same"); 
    }
  }

  unsigned int nAdd = internalAdds.size();
  for (unsigned int iA=0; iA!=nAdd; ++iA) {
    ObjWrapper w = internalAdds[iA];
    TObject *o = w.o;
    TString opt = w.opt;
    TString className(o->ClassName());    
    if (className.Contains("TH1"))
      o->Draw(opt+" same");
    else if (className.Contains("TGraph"))
      o->Draw(opt+" same");
    else if (className.Contains("TF1"))
      o->Draw(opt+" same");
    else {
      printf("Don't know what to do with %s\n",className.Data());
      continue;
    }

    TString label = w.label;
    if (label.Length()>0) {
      legend->AddEntry(o,label,"l");
    }
  }

  if (hData!=NULL && firstHist!=hData) {
    hData->Draw("elp same"); 
  }

  if (legend)
    legend->Draw();

  c->cd();

  TGraphErrors *gRatioErrors;
  double *xVals=0, *yVals=0, *xErrors=0, *yErrors=0;
  if (doRatio) {
    pad2 = new TPad("pad2", "pad2", 0, 0.05, 1, 0.3);
    pad2->SetTopMargin(0);
    pad2->SetBottomMargin(0.3);
    pad2->Draw();
    pad2->cd();
    hRatio = (TH1F*)hData->Clone("ratio");
    if (doDrawMCErrors)
      hRatioError = (TH1F*)hSum->Clone("sumratio");
    hZero = (TH1F*)hData->Clone("zero"); 
    for (int iB=0; iB!=nBins; ++iB) 
      hZero->SetBinContent(iB+1,1);
    
    float maxVal=0;
    xVals = new double[nBins];
    yVals = new double[nBins];
    xErrors = new double[nBins];
    yErrors = new double[nBins];
    for (int iB=1; iB!=nBins+1; ++iB) {
      float mcVal=0;
      for (HistWrapper w : hOthers) {
        TH1F *hh = w.h;
        mcVal += hh->GetBinContent(iB);
      }
      float dataVal = hData->GetBinContent(iB);
      float err = hData->GetBinError(iB);
      float mcErr = 0;
      if (doDrawMCErrors)
        mcErr = hSum->GetBinError(iB);
      float val,errVal,mcErrVal;
      if (dataVal==0.||mcVal==0.) {
        if (dataVal>0) 
          fprintf(stderr,"WARNING, BIN %i HAS DATA=%.1f, but EXP=%.3f\n",iB,dataVal,mcVal);
        if (mcVal==0) 
          mcErrVal=0;
        else
          mcErrVal = mcErr/mcVal;
        val=0;
        errVal=0;
      } else {
        val = dataVal/mcVal;
        errVal = err/mcVal;
        mcErrVal = mcErr/mcVal;
      }
      if (val==0.) val=-999; // don't plot missing data points
      xVals[iB-1] = hRatio->GetBinCenter(iB);
      yVals[iB-1] = val;
      xErrors[iB-1] = 0;
      yErrors[iB-1] = errVal;
      maxVal = std::max(maxVal,std::abs(val-1));
      maxVal = std::max(maxVal,std::abs(errVal+val-1));
      maxVal = std::max(maxVal,std::abs(1-val-errVal));
      hRatio->SetBinContent(iB,val);
      hRatio->SetBinError(iB,0.0001);
      if (doDrawMCErrors) {
        hRatioError->SetBinContent(iB,1);
        hRatioError->SetBinError(iB,mcErrVal);
      }
    } 
    gRatioErrors = new TGraphErrors(nBins,xVals,yVals,xErrors,yErrors);
    maxVal = std::min((double)maxVal,1.5);
    if (fixRatio && ratioMax>0)
      maxVal = ratioMax;
    maxVal = std::max(double(maxVal),0.6);
    hRatio->SetTitle("");
    hRatio->Draw("elp");
    hRatio->SetMinimum(-1.2*maxVal+1);
    hRatio->SetMaximum(maxVal*1.2+1);
    hRatio->SetLineColor(1);
    hRatio->SetMarkerStyle(20);
    hRatio->GetXaxis()->SetTitle(xlabel);
    hRatio->GetYaxis()->SetTitle("Data/Exp");
    hRatio->GetYaxis()->SetNdivisions(5);
    hRatio->GetYaxis()->SetTitleSize(15);
    hRatio->GetYaxis()->SetTitleFont(43);
    hRatio->GetYaxis()->SetTitleOffset(1.55);
    hRatio->GetYaxis()->SetLabelFont(43); 
    hRatio->GetYaxis()->SetLabelSize(15);
    hRatio->GetXaxis()->SetTitleSize(20);
    hRatio->GetXaxis()->SetTitleFont(43);
    hRatio->GetXaxis()->SetTitleOffset(4.);
    hRatio->GetXaxis()->SetLabelFont(43);
    hRatio->GetXaxis()->SetLabelSize(15);
    gRatioErrors->SetLineWidth(2);
    gRatioErrors->Draw("p");
    hZero->SetLineColor(1);
    hZero->Draw("hist same");
    if (doDrawMCErrors)
      hRatioError->Draw("e2 same");
  }
  if (doRatio)
    pad1->cd();

  CanvasDrawer::Draw(outDir,baseName);
  delete hSum; hSum=0;
  delete hRatioError; hRatioError=0;
  delete hs; hs=0;
  delete hRatio; hRatio=0;
  delete hZero; hZero=0;
  delete pad1; pad1=0;
  delete pad2; pad2=0;
  delete hs; hs=0;
  hOthers.clear();
}
     
