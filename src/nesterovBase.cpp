#include "opendb/db.h"

#include "nesterovBase.h"
#include "placerBase.h"
#include "fft.h"

#include <algorithm>
#include <iostream>
#include <random>


#define REPLACE_SQRT2 1.414213562373095048801L

namespace replace {

using namespace std;
using namespace odb;

static int 
fastModulo(const int input, const int ceil);

static int32_t 
getOverlapArea(Bin* bin, GCell* cell);

static int32_t
getOverlapArea(Bin* bin, Instance* inst);

static int32_t
getOverlapDensityArea(Bin* bin, GCell* cell);

static float
fastExp(float exp);


////////////////////////////////////////////////
// GCell 

GCell::GCell() 
  : lx_(0), ly_(0), ux_(0), uy_(0),
  dLx_(0), dLy_(0), dUx_(0), dUy_(0),
  densityScale_(0), 
  gradientX_(0), gradientY_(0) {}


GCell::GCell(Instance* inst) 
  : GCell() {
  setInstance(inst);
}

GCell::GCell(std::vector<Instance*>& insts) 
  : GCell() {
  setClusteredInstance(insts);
}

GCell::GCell(int cx, int cy, int dx, int dy) 
  : GCell() {
  lx_ = cx - dx/2;
  ly_ = cy - dy/2;
  ux_ = cx + dx/2;
  uy_ = cy + dy/2; 
  setFiller();
}

GCell::~GCell() {
  vector<Instance*> ().swap(insts_);
}

void
GCell::setInstance(Instance* inst) {
  insts_.push_back(inst);
}

Instance*
GCell::instance() const {
  return *insts_.begin();
}

void
GCell::addGPin(GPin* gPin) {
  gPins_.push_back(gPin);
}


// do nothing
void
GCell::setFiller() {
}

void
GCell::setClusteredInstance(std::vector<Instance*>& insts) {
  insts_ = insts;
}

int
GCell::lx() const {
  return lx_;
}
int
GCell::ly() const {
  return ly_; 
}

int
GCell::ux() const { 
  return ux_; 
}

int
GCell::uy() const {
  return uy_; 
}

int
GCell::cx() const {
  return (lx_ + ux_)/2;
}

int
GCell::cy() const {
  return (ly_ + uy_)/2;
}

int 
GCell::dx() const {
  return ux_ - lx_; 
}

int
GCell::dy() const {
  return uy_ - ly_;
}

int
GCell::dLx() const {
  return dLx_;
}

int
GCell::dLy() const {
  return dLy_;
}

int
GCell::dUx() const {
  return dUx_;
}

int
GCell::dUy() const {
  return dUy_;
}

int
GCell::dCx() const {
  return (dUx_ - dLx_)/2;
}

int
GCell::dCy() const {
  return (dUy_ - dLy_)/2;
}

int
GCell::dDx() const {
  return dUx_ - dLx_; 
}

int
GCell::dDy() const {
  return dUy_ - dLy_;
}

void
GCell::setLocation(int lx, int ly) {
  ux_ = lx + (ux_ - lx_);
  uy_ = ly + (uy_ - ly_);
  lx = lx_;
  ly = ly_;

  for(auto& gPin: gPins_) {
    gPin->updateLocation(this);
  }
}

void
GCell::setCenterLocation(int cx, int cy) {
  const int halfDx = dx()/2;
  const int halfDy = dy()/2;

  lx_ = cx - halfDx;
  ly_ = cy - halfDy;
  ux_ = cx + halfDx;
  uy_ = cy + halfDy;

  for(auto& gPin: gPins_) {
    gPin->updateLocation(this);
  }
}

// changing size and preserve center coordinates
void
GCell::setSize(int dx, int dy) {
  const int centerX = cx();
  const int centerY = cy();

  lx_ = centerX - dx/2;
  ly_ = centerY - dy/2;
  ux_ = centerX + dx/2;
  uy_ = centerY + dy/2;
}

void
GCell::setDensityLocation(int dLx, int dLy) {
  dUx_ = dLx + (dUx_ - dLx_);
  dUy_ = dLy + (dUy_ - dLy_);
  dLx_ = dLx;
  dLy_ = dLy;
}

void
GCell::setDensityCenterLocation(int dCx, int dCy) {
  const int halfDDx = dDx()/2;
  const int halfDDy = dDy()/2;

  dLx_ = dCx - halfDDx;
  dLy_ = dCy - halfDDy;
  dUx_ = dCx + halfDDx;
  dUy_ = dCy + halfDDy;
}

// changing size and preserve center coordinates
void
GCell::setDensitySize(int dDx, int dDy) {
  const int dCenterX = dCx();
  const int dCenterY = dCy();

  dLx_ = dCenterX - dDx/2;
  dLy_ = dCenterY - dDy/2;
  dUx_ = dCenterX + dDx/2;
  dUy_ = dCenterY + dDy/2;
}

void
GCell::setDensityScale(float densityScale) {
  densityScale_ = densityScale;
}

void
GCell::setGradientX(float gradientX) {
  gradientX_ = gradientX;
}

void
GCell::setGradientY(float gradientY) {
  gradientY_ = gradientY;
}

bool 
GCell::isInstance() const {
  return (insts_.size() == 1);
}

bool
GCell::isClusteredInstance() const {
  return (insts_.size() > 0);
}

bool
GCell::isFiller() const {
  return (insts_.size() == 0);
}

////////////////////////////////////////////////
// GNet

GNet::GNet()
  : lx_(0), ly_(0), ux_(0), uy_(0),
  customWeight_(1), weight_(1),
  waExpMinSumStorX_(0), waXExpMinSumStorX_(0),
  waExpMaxSumStorX_(0), waXExpMaxSumStorX_(0),
  waExpMinSumStorY_(0), waYExpMinSumStorY_(0),
  waExpMaxSumStorY_(0), waYExpMaxSumStorY_(0),
  isDontCare_(0) {}

GNet::GNet(Net* net) : GNet() {
  nets_.push_back(net);
}

GNet::GNet(std::vector<Net*>& nets) : GNet() {
  nets_ = nets;
}

GNet::~GNet() {
  gPins_.clear();
  nets_.clear();
}

Net* 
GNet::net() const { 
  return *nets_.begin();
}

void
GNet::setCustomWeight(float customWeight) {
  customWeight_ = customWeight; 
}

void
GNet::addGPin(GPin* gPin) {
  gPins_.push_back(gPin);
}

void
GNet::updateBox() {
  lx_ = ly_ = INT_MAX;
  ux_ = uy_ = INT_MIN;

  for(auto& gPin : gPins_) {
    lx_ = std::min(gPin->cx(), lx_);
    ly_ = std::min(gPin->cy(), ly_);
    ux_ = std::max(gPin->cx(), ux_);
    uy_ = std::max(gPin->cy(), uy_);
  } 
}

// eight add functions
void
GNet::addWaExpMinSumX(float waExpMinSumX) {
  waExpMinSumStorX_ += waExpMinSumX;
}

void
GNet::addWaXExpMinSumX(float waXExpMinSumX) {
  waXExpMinSumStorX_ += waXExpMinSumX;
}

void
GNet::addWaExpMinSumY(float waExpMinSumY) {
  waExpMinSumStorY_ += waExpMinSumY;
}

void
GNet::addWaYExpMinSumY(float waYExpMinSumY) {
  waYExpMinSumStorY_ += waYExpMinSumY;
}

void
GNet::addWaExpMaxSumX(float waExpMaxSumX) {
  waExpMaxSumStorX_ += waExpMaxSumX;
}

void
GNet::addWaXExpMaxSumX(float waXExpMaxSumX) {
  waXExpMaxSumStorX_ += waXExpMaxSumX;
}

void
GNet::addWaExpMaxSumY(float waExpMaxSumY) {
  waExpMaxSumStorY_ += waExpMaxSumY;
}

void
GNet::addWaYExpMaxSumY(float waYExpMaxSumY) {
  waYExpMaxSumStorY_ += waYExpMaxSumY;
}

void
GNet::setDontCare() {
  isDontCare_ = 1;
}

bool
GNet::isDontCare() {
  return (gPins_.size() == 0) || (isDontCare_ == 1);
}

////////////////////////////////////////////////
// GPin 

GPin::GPin()
  : gCell_(nullptr), gNet_(nullptr),
  offsetCx_(0), offsetCy_(0),
  cx_(0), cy_(0),
  maxExpSumX_(0), maxExpSumY_(0),
  minExpSumX_(0), minExpSumY_(0),
  hasMaxExpSumX_(0), hasMaxExpSumY_(0), 
  hasMinExpSumX_(0), hasMinExpSumY_(0) {}

GPin::GPin(Pin* pin)
  : GPin() {
  pins_.push_back(pin);
  cx_ = pin->cx();
  cy_ = pin->cy();
  offsetCx_ = pin->offsetCx();
  offsetCy_ = pin->offsetCy();
}

GPin::GPin(std::vector<Pin*> & pins) {
  pins_ = pins;
}

GPin::~GPin() {
  gCell_ = nullptr;
  gNet_ = nullptr;
  pins_.clear();
}

Pin* 
GPin::pin() const {
  return *pins_.begin();
}

void
GPin::setGCell(GCell* gCell) {
  gCell_ = gCell;
}

void
GPin::setGNet(GNet* gNet) {
  gNet_ = gNet;
}

void
GPin::setCenterLocation(int cx, int cy) {
  cx_ = cx;
  cy_ = cy;
}

void
GPin::setMaxExpSumX(float maxExpSumX) {
  hasMaxExpSumX_ = 1;
  maxExpSumX_ = maxExpSumX;
}

void
GPin::setMaxExpSumY(float maxExpSumY) {
  hasMaxExpSumY_ = 1;
  maxExpSumY_ = maxExpSumY;
}

void
GPin::setMinExpSumX(float minExpSumX) {
  hasMinExpSumX_ = 1;
  minExpSumX_ = minExpSumX;
}

void
GPin::setMinExpSumY(float minExpSumY) {
  hasMinExpSumY_ = 1;
  minExpSumY_ = minExpSumY;
}


void
GPin::updateLocation(const GCell* gCell) {
  cx_ = gCell->cx() + offsetCx_;
  cy_ = gCell->cy() + offsetCy_;
}

////////////////////////////////////////////////////////
// Bin

Bin::Bin() 
  : x_(0), y_(0), lx_(0), ly_(0),
  ux_(0), uy_(0), 
  nonPlaceArea_(0), placedArea_(0),
  fillerArea_(0),
  phi_(0), density_(0) {}

Bin::Bin(int x, int y, int lx, int ly, int ux, int uy) 
  : Bin() {
  x_ = x;
  y_ = y;
  lx_ = lx; 
  ly_ = ly;
  ux_ = ux;
  uy_ = uy;
}

Bin::~Bin() {
  x_ = y_ = 0;
  lx_ = ly_ = ux_ = uy_ = 0;
  nonPlaceArea_ = placedArea_ = fillerArea_ = 0;
  phi_ = density_ = electroForce_ = 0;
}

int
Bin::x() const {
  return x_;
}

int
Bin::y() const { 
  return y_;
}

int 
Bin::lx() const {
  return lx_;
}
int
Bin::ly() const { 
  return ly_;
}

int
Bin::ux() const { 
  return ux_;
}

int
Bin::uy() const { 
  return uy_;
}

int
Bin::cx() const { 
  return (ux_ - lx_)/2;
}

int
Bin::cy() const { 
  return (uy_ - ly_)/2;
}

int
Bin::dx() const { 
  return (ux_ - lx_);
} 
int
Bin::dy() const { 
  return (uy_ - ly_);
}

void
Bin::setNonPlaceArea(int64_t area) {
  nonPlaceArea_ = area;
}

void
Bin::setPlacedArea(int64_t area) {
  placedArea_ = area;
}

void
Bin::setFillerArea(int64_t area) {
  fillerArea_ = area;
}

void
Bin::addNonPlaceArea(int64_t area) {
  nonPlaceArea_ += area;
}

void
Bin::addPlacedArea(int64_t area) {
  placedArea_ += area;
}

void
Bin::addFillerArea(int64_t area) {
  fillerArea_ += area;
}


float
Bin::phi() const {
  return phi_;
}

float
Bin::density() const {
  return density_;
}

float
Bin::electroForce() const {
  return electroForce_;
}

void
Bin::setPhi(float phi) {
  phi_ = phi;
}

void
Bin::setDensity(float density) {
  density_ = density;
}

void
Bin::setElectroForce(float electroForce) {
  electroForce_ = electroForce;
}

////////////////////////////////////////////////
// BinGrid

BinGrid::BinGrid()
  : lx_(0), ly_(0), ux_(0), uy_(0),
  binCntX_(0), binCntY_(0),
  binSizeX_(0), binSizeY_(0),
  targetDensity_(0), 
  isSetBinCntX_(0), isSetBinCntY_(0) {}

BinGrid::BinGrid(Die* die) : BinGrid() {
  setCoordi(die);
}

BinGrid::~BinGrid() {
  std::vector<Bin> ().swap(binStor_);
  std::vector<Bin*> ().swap(bins_);
  lx_ = ly_ = ux_ = uy_ = 0;
  binCntX_ = binCntY_ = 0;
  binSizeX_ = binSizeY_ = 0;
  isSetBinCntX_ = isSetBinCntY_ = 0;
}

void
BinGrid::setCoordi(Die* die) {
  lx_ = die->coreLx();
  ly_ = die->coreLy();
  ux_ = die->coreUx();
  uy_ = die->coreUy();
}

void
BinGrid::setPlacerBase(std::shared_ptr<PlacerBase> pb) {
  pb_ = pb;
}

void
BinGrid::setTargetDensity(float density) {
  targetDensity_ = density;
}

void
BinGrid::setBinCnt(int binCntX, int binCntY) {
  setBinCntX(binCntX);
  setBinCntY(binCntY);
}

void
BinGrid::setBinCntX(int binCntX) {
  isSetBinCntX_ = 1;
  binCntX_ = binCntX;
}

void
BinGrid::setBinCntY(int binCntY) {
  isSetBinCntY_ = 1;
  binCntY_ = binCntY;
}


int
BinGrid::lx() const {
  return lx_;
}
int
BinGrid::ly() const { 
  return ly_;
}

int
BinGrid::ux() const { 
  return ux_;
}

int
BinGrid::uy() const { 
  return uy_;
}

int
BinGrid::cx() const { 
  return (ux_ - lx_)/2;
}

int
BinGrid::cy() const { 
  return (uy_ - ly_)/2;
}

int
BinGrid::dx() const { 
  return (ux_ - lx_);
} 
int
BinGrid::dy() const { 
  return (uy_ - ly_);
}

int
BinGrid::binCntX() const {
  return binCntX_;
}

int
BinGrid::binCntY() const {
  return binCntY_;
}

int
BinGrid::binSizeX() const {
  return binSizeX_;
}

int
BinGrid::binSizeY() const {
  return binSizeY_;
}

void
BinGrid::initBins() {

  int64_t totalBinArea 
    = static_cast<int64_t>(ux_ - lx_) 
    * static_cast<int64_t>(uy_ - ly_);

  int32_t averagePlaceInstArea 
    = pb_->placeInstsArea() / pb_->placeInsts().size();

  int64_t idealBinArea = 
    std::round(static_cast<float>(averagePlaceInstArea) / targetDensity_);
  int idealBinCnt = totalBinArea / idealBinArea; 
  
  cout << "TargetDensity  : " << targetDensity_ << endl;
  cout << "AveragePlaceInstArea: : " << averagePlaceInstArea << endl;
  cout << "IdealBinArea   : " << idealBinArea << endl;
  cout << "IdealBinCnt    : " << idealBinCnt << endl;
  cout << "TotalBinArea   : " << totalBinArea << endl;

  int foundBinCnt = 2;
  // find binCnt: 2, 4, 8, 16, 32, 64, ...
  // s.t. binCnt^2 <= idealBinCnt <= (binCnt*2)^2.
  for(foundBinCnt = 2; foundBinCnt <= 1024; foundBinCnt *= 2) {
    if( foundBinCnt * foundBinCnt <= idealBinCnt 
        && 4 * foundBinCnt * foundBinCnt > idealBinCnt ) {
      break;
    }
  }

  // setBinCntX_;
  if( !isSetBinCntX_ ) {
    binCntX_ = foundBinCnt;
  }

  // setBinCntY_;
  if( !isSetBinCntY_ ) {
    binCntY_ = foundBinCnt;
  }

  cout << "BinCnt         : ( " << binCntX_ 
    << ", " << binCntY_ << " )" << endl;
  
  binSizeX_ = ceil(
      static_cast<float>((ux_ - lx_))/binCntX_);
  binSizeY_ = ceil(
      static_cast<float>((uy_ - ly_))/binCntY_);
  
  cout << "BinSize        : ( " << binSizeX_
    << ", " << binSizeY_ << " )" << endl;

  // initialize binStor_, bins_ vector
  binStor_.resize(binCntX_ * binCntY_);
  int x = lx_, y = ly_;
  int idxX = 0, idxY = 0;
  for(auto& bin : binStor_) {

    int sizeX = (x + binSizeX_ > ux_)? 
      ux_ - x : binSizeX_;
    int sizeY = (y + binSizeY_ > uy_)? 
      uy_ - y : binSizeY_;

    //cout << x << " " << y 
    //  << " " << x+sizeX << " " << y+sizeY << endl;
    bin = Bin(idxX, idxY, x, y, x+sizeX, y+sizeY);
    
    // move x, y coordinates.
    x += binSizeX_;
    idxX += 1;

    if( x > ux_ ) {
      y += binSizeY_;
      x = lx_; 
      
      idxY ++;
      idxX = 0;
    }

    bins_.push_back( &bin );
  }

  cout << "TotalBinCnt    : " << bins_.size() << endl;

  // only initialized once
  updateBinsNonPlaceArea();
}

void
BinGrid::updateBinsNonPlaceArea() {
  for(auto& bin : bins_) {
    bin->setNonPlaceArea(0);
  }

  for(auto& inst : pb_->nonPlaceInsts()) {
    std::pair<int, int> pairX = getMinMaxIdxX(inst);
    std::pair<int, int> pairY = getMinMaxIdxY(inst);
   
    for(int i = pairX.first; i <= pairX.second; i++) {
      for(int j = pairY.first; j <= pairY.second; j++) {
        Bin* bin = bins_[ j * binCntX_ + i ];
        bin->addNonPlaceArea( getOverlapArea(bin, inst) );
      }
    }
  }
}


// Core Part
void
BinGrid::updateBinsGCellArea(std::vector<GCell*>& cells) {
  // clear the Bin-area info
  for(auto& bin : bins_) {
    bin->setPlacedArea(0);
    bin->setFillerArea(0);
  }

  for(auto& cell : cells) {
    std::pair<int, int> pairX = getMinMaxIdxX(cell);
    std::pair<int, int> pairY = getMinMaxIdxY(cell);
   
    if( cell->isInstance() ) {
      for(int i = pairX.first; i <= pairX.second; i++) {
        for(int j = pairY.first; j <= pairY.second; j++) {
          Bin* bin = bins_[ j * binCntX_ + i ];
          bin->addPlacedArea( getOverlapArea(bin, cell) );
        }
      }
    }
    else if( cell->isFiller() ) {
      for(int i = pairX.first; i <= pairX.second; i++) {
        for(int j = pairY.first; j <= pairY.second; j++) {
          Bin* bin = bins_[ j * binCntX_ + i ];
          bin->addFillerArea( getOverlapArea(bin, cell) );
        }
      }
    }
  }  
}

// Core Part
void
BinGrid::updateBinsGCellDensityArea(
    std::vector<GCell*>& cells) {
  // clear the Bin-area info
  for(auto& bin : bins_) {
    bin->setPlacedArea(0);
    bin->setFillerArea(0);
  }

  for(auto& cell : cells) {
    std::pair<int, int> pairX = getMinMaxIdxX(cell);
    std::pair<int, int> pairY = getMinMaxIdxY(cell);
   
    if( cell->isInstance() ) {
      for(int i = pairX.first; i <= pairX.second; i++) {
        for(int j = pairY.first; j <= pairY.second; j++) {
          Bin* bin = bins_[ j * binCntX_ + i ];
          bin->addPlacedArea( getOverlapDensityArea(bin, cell) ); 
        }
      }
    }
    else if( cell->isFiller() ) {
      for(int i = pairX.first; i <= pairX.second; i++) {
        for(int j = pairY.first; j <= pairY.second; j++) {
          Bin* bin = bins_[ j * binCntX_ + i ];
          bin->addFillerArea( getOverlapDensityArea(bin, cell) ); 
        }
      }
    }
  }  
}


std::pair<int, int>
BinGrid::getMinMaxIdxX(GCell* gcell) {
  int lowerIdx = (gcell->lx() - lx())/binSizeX_;
  int upperIdx = 
   ( fastModulo((gcell->ux() - lx()), binSizeX_) == 0)? 
   (gcell->ux() - lx()) / binSizeX_ 
   : (gcell->ux() - lx()) / binSizeX_ + 1;
  return std::make_pair(lowerIdx, upperIdx);
}

std::pair<int, int>
BinGrid::getMinMaxIdxY(GCell* gcell) {
  int lowerIdx = (gcell->ly() - ly())/binSizeY_;
  int upperIdx =
   ( fastModulo((gcell->uy() - ly()), binSizeY_) == 0)? 
   (gcell->uy() - ly()) / binSizeY_ 
   : (gcell->uy() - ly()) / binSizeY_ + 1;
  return std::make_pair(lowerIdx, upperIdx);
}


std::pair<int, int>
BinGrid::getMinMaxIdxX(Instance* inst) {
  int lowerIdx = (inst->lx() - lx()) / binSizeX_;
  int upperIdx = 
   ( fastModulo((inst->ux() - lx()), binSizeX_) == 0)? 
   (inst->ux() - lx()) / binSizeX_ 
   : (inst->ux() - lx()) / binSizeX_ + 1;
  return std::make_pair(lowerIdx, upperIdx);
}

std::pair<int, int>
BinGrid::getMinMaxIdxY(Instance* inst) {
  int lowerIdx = (inst->ly() - ly()) / binSizeY_;
  int upperIdx = 
   ( fastModulo((inst->uy() - ly()), binSizeY_) == 0)? 
   (inst->uy() - ly()) / binSizeY_ 
   : (inst->uy() - ly()) / binSizeY_ + 1;
  return std::make_pair(lowerIdx, upperIdx);
}




////////////////////////////////////////////////
// NesterovBaseVars
NesterovBaseVars::NesterovBaseVars() 
: targetDensity(1.0), 
  minAvgCut(0.1), maxAvgCut(0.9),
isSetBinCntX(0), isSetBinCntY(0), 
  binCntX(0), binCntY(0),
  minWireLengthForceBar(-300) {}



void 
NesterovBaseVars::reset() {
  targetDensity = 1.0;
  minAvgCut = 0.1;
  maxAvgCut = 0.9;
  isSetBinCntX = isSetBinCntY = 0;
  binCntX = binCntY = 0;
  minWireLengthForceBar = -300;
}


////////////////////////////////////////////////
// NesterovBase 

NesterovBase::NesterovBase()
  : pb_(nullptr) {}

NesterovBase::NesterovBase(
    NesterovBaseVars nbVars, 
    std::shared_ptr<PlacerBase> pb)
  : NesterovBase() {
  nbVars_ = nbVars;
  pb_ = pb;
  init();
}

NesterovBase::~NesterovBase() {
  pb_ = nullptr;
}

void
NesterovBase::init() {
  // gCellStor init
  gCellStor_.reserve(pb_->insts().size());
  for(auto& inst: pb_->insts()) {
    GCell myGCell(inst); 
    gCellStor_.push_back(myGCell);
  }

  cout << "InstGCells     : " 
    << gCellStor_.size() << endl;
  
  // TODO: 
  // at this moment, GNet and GPin is equal to
  // Net and Pin

  // gPinStor init
  gPinStor_.reserve(pb_->pins().size());
  for(auto& pin : pb_->pins()) {
    GPin myGPin(pin);
    gPinStor_.push_back(myGPin);
  }

  // gNetStor init
  gNetStor_.reserve(pb_->nets().size());
  for(auto& net : pb_->nets()) {
    GNet myGNet(net);
    gNetStor_.push_back(myGNet);
  }


  // update gFillerCells
  initFillerGCells();

  // gCell ptr init
  gCells_.reserve(gCellStor_.size());
  for(auto& gCell : gCellStor_) {
    gCells_.push_back(&gCell);
    if( gCell.isInstance() ) {
      gCellMap_[gCell.instance()] = &gCell;
    }
  }
  
  // gPin ptr init
  gPins_.reserve(gPinStor_.size());
  for(auto& gPin : gPinStor_) {
    gPins_.push_back(&gPin);
    gPinMap_[gPin.pin()] = &gPin;
  }

  // gNet ptr init
  gNets_.reserve(gNetStor_.size());
  for(auto& gNet : gNetStor_) {
    gNets_.push_back(&gNet);
    gNetMap_[gNet.net()] = &gNet;
  }

  // gCellStor_'s pins_ fill
  for(auto& gCell : gCellStor_) {
    if( gCell.isFiller()) {
      continue;
    }

    for( auto& pin : gCell.instance()->pins() ) {
      gCell.addGPin( placerToNesterov(pin) );
    }
  }

  // gPinStor_' GNet and GCell fill
  for(auto& gPin : gPinStor_) {
    gPin.setGCell( 
        placerToNesterov(gPin.pin()->instance()));
    gPin.setGNet(
        placerToNesterov(gPin.pin()->net()));
  } 

  // gNetStor_'s GPin fill
  for(auto& gNet : gNetStor_) {
    for(auto& pin : gNet.net()->pins()) {
      gNet.addGPin( placerToNesterov(pin) );
    }
  }

  cout << "GCells         : " 
    << gCells_.size() << endl;
  cout << "GNets          : " 
    << gNets_.size() << endl;
  cout << "GPins          : " 
    << gPins_.size() << endl;

  // initialize bin grid structure
  // send param into binGrid structure
  if( nbVars_.isSetBinCntX ) {
    bg_.setBinCntX(nbVars_.binCntX);
  }
  
  if( nbVars_.isSetBinCntY ) {
    bg_.setBinCntY(nbVars_.binCntY);
  }

  bg_.setPlacerBase(pb_);
  bg_.setCoordi(&(pb_->die()));
  bg_.setTargetDensity(nbVars_.targetDensity);
  
  // update binGrid info
  bg_.initBins();


  // initialize fft structrue based on bins
  std::unique_ptr<FFT> fft(new FFT(bg_.binCntX(), bg_.binCntY(), bg_.binSizeX(), bg_.binSizeY()));
  fft_ = std::move(fft);


  // update densitySize and densityScale in each gCell
  for(auto& gCell : gCells_) {
    float scaleX = 0, scaleY = 0;
    float densitySizeX = 0, densitySizeY = 0;
    if( gCell->dx() < REPLACE_SQRT2 * bg_.binSizeX() ) {
      scaleX = static_cast<float>(gCell->dx()) / 
        static_cast<float>( REPLACE_SQRT2 * bg_.binSizeX());
      densitySizeX = REPLACE_SQRT2 * static_cast<float>(bg_.binSizeX()) / 2.0;
    }
    else {
      scaleX = 1.0;
      densitySizeX = gCell->dx();
    }

    if( gCell->dy() < REPLACE_SQRT2 * bg_.binSizeY() ) {
      scaleY = static_cast<float>(gCell->dy()) / 
        static_cast<float>( REPLACE_SQRT2 * bg_.binSizeY());
      densitySizeY = REPLACE_SQRT2 * static_cast<float>(bg_.binSizeY()) / 2.0;
    }
    else {
      scaleY = 1.0;
      densitySizeY = gCell->dy();
    }

    gCell->setDensitySize(densitySizeX, densitySizeY);
    gCell->setDensityScale(scaleX * scaleY);
  } 
}


// virtual filler GCells
void
NesterovBase::initFillerGCells() {
  // extract average dx/dy in range (10%, 90%)
  vector<int> dxStor;
  vector<int> dyStor;

  dxStor.reserve(pb_->placeInsts().size());
  dyStor.reserve(pb_->placeInsts().size());
  for(auto& placeInst : pb_->placeInsts()) {
    dxStor.push_back(placeInst->dx());
    dyStor.push_back(placeInst->dy());
  }
  
  // sort
  std::sort(dxStor.begin(), dxStor.end());
  std::sort(dyStor.begin(), dyStor.end());

  // average from (10 - 90%) .
  uint32_t dxSum = 0, dySum = 0;

  int minIdx = dxStor.size()*0.10;
  int maxIdx = dxStor.size()*0.90;
  for(int i=minIdx; i<maxIdx; i++) {
    dxSum += dxStor[i];
    dySum += dyStor[i];
  }

  // the avgDx and avgDy will be used as filler cells' 
  // width and height
  int avgDx = static_cast<int>(dxSum / (maxIdx - minIdx));
  int avgDy = static_cast<int>(dySum / (maxIdx - minIdx));

  cout << "FillerSize     : ( " 
    << avgDx << ", " << avgDy << " )" << endl;

  int64_t coreArea = 
    static_cast<int64_t>(pb_->die().coreDx()) *
    static_cast<int64_t>(pb_->die().coreDy()); 

  int64_t whiteSpaceArea = coreArea - 
    static_cast<int64_t>(pb_->nonPlaceInstsArea());

  int64_t movableArea = whiteSpaceArea * nbVars_.targetDensity;
  int64_t totalFillerArea = movableArea 
    - static_cast<int64_t>(pb_->placeInstsArea());

  if( totalFillerArea < 0 ) {
    cout << "ERROR: Filler area is negative!!" << endl;
    cout << "       Please put higher target density or " << endl;
    cout << "       Re-floorplan to have enough coreArea" << endl;
    exit(1);
  }

  int fillerCnt = 
    static_cast<int>(totalFillerArea 
        / static_cast<int32_t>(avgDx * avgDy));

  cout << "FillerGCells   : " << fillerCnt << endl;

  // 
  // mt19937 supports huge range of random values.
  // rand()'s RAND_MAX is only 32767.
  //
  mt19937 randVal(0);
  for(int i=0; i<fillerCnt; i++) {
    // place filler cells on random coordi and
    // set size as avgDx and avgDy
    GCell myGCell(
        randVal() % pb_->die().coreDx() + pb_->die().coreLx(), 
        randVal() % pb_->die().coreDy() + pb_->die().coreLy(),
        avgDx, avgDy );

    gCellStor_.push_back(myGCell);
  }
}

GCell*
NesterovBase::placerToNesterov(Instance* inst) {
  auto gcPtr = gCellMap_.find(inst);
  return (gcPtr == gCellMap_.end())?
    nullptr : gcPtr->second;
}

GNet*
NesterovBase::placerToNesterov(Net* net) {
  auto gnPtr = gNetMap_.find(net);
  return (gnPtr == gNetMap_.end())?
    nullptr : gnPtr->second;
}

GPin*
NesterovBase::placerToNesterov(Pin* pin) {
  auto gpPtr = gPinMap_.find(pin);
  return (gpPtr == gPinMap_.end())?
    nullptr : gpPtr->second;
}

// gcell update
void
NesterovBase::updateGCellLocation(
    std::vector<FloatCoordi>& coordis) {
  for(auto& coordi : coordis) {
    int idx = &coordi - &coordis[0];
    gCells_[idx]->setLocation( coordi.x, coordi.y );
  }
}

// gcell update
void
NesterovBase::updateGCellCenterLocation(
    std::vector<FloatCoordi>& coordis) {
  for(auto& coordi : coordis) {
    int idx = &coordi - &coordis[0];
    gCells_[idx]->setCenterLocation( coordi.x, coordi.y );
  }
}

void
NesterovBase::updateGCellDensityCenterLocation(
    std::vector<FloatCoordi>& coordis) {
  for(auto& coordi : coordis) {
    int idx = &coordi - &coordis[0];
    gCells_[idx]->setDensityCenterLocation( 
        coordi.x, coordi.y );
  }
}

// 
// WA force cals - wlCoeffX / wlCoeffY
//
// * Note that wlCoeffX and wlCoeffY is 1/gamma 
// in ePlace paper.
void
NesterovBase::updateWireLengthForceWA(
    float wlCoeffX, float wlCoeffY) {

  for(auto& gNet : gNets_) {
    gNet->updateBox();

    for(auto& gPin : gNet->gPins()) {
      float expMinX = (gNet->lx() - gPin->cx()) * wlCoeffX; 
      float expMaxX = (gPin->cx() - gNet->ux()) * wlCoeffX;
      float expMinY = (gNet->ly() - gPin->cy()) * wlCoeffY;
      float expMaxY = (gPin->cy() - gNet->ly()) * wlCoeffY;

      // min x
      if(expMinX > nbVars_.minWireLengthForceBar) {
        gPin->setMinExpSumX( fastExp(expMinX) );
        gNet->addWaExpMinSumX( gPin->minExpSumX() );
        gNet->addWaXExpMinSumX( gPin->cx() 
            * gPin->minExpSumX() );
      }
      
      // max x
      if(expMaxX > nbVars_.minWireLengthForceBar) {
        gPin->setMaxExpSumX( fastExp(expMaxX) );
        gNet->addWaExpMaxSumX( gPin->maxExpSumX() );
        gNet->addWaXExpMaxSumX( gPin->cx() 
            * gPin->maxExpSumX() );
      }
     
      // min y 
      if(expMinY > nbVars_.minWireLengthForceBar) {
        gPin->setMinExpSumY( fastExp(expMinY) );
        gNet->addWaExpMinSumY( gPin->minExpSumY() );
        gNet->addWaYExpMinSumY( gPin->cy() 
            * gPin->minExpSumY() );
      }
      
      // max y
      if(expMaxY > nbVars_.minWireLengthForceBar) {
        gPin->setMaxExpSumY( fastExp(expMaxY) );
        gNet->addWaExpMaxSumY( gPin->maxExpSumY() );
        gNet->addWaYExpMaxSumY( gPin->cy() 
            * gPin->maxExpSumY() );
      }
    }
    //cout << gNet->lx() << " " << gNet->ly() << " "
    //  << gNet->ux() << " " << gNet->uy() << endl;
  }
}

// get x,y WA Gradient values with given GCell
FloatCoordi
NesterovBase::getWireLengthGradientWA(GCell* gCell, float wlCoeffX, float wlCoeffY) {
  FloatCoordi gradientPair;

  for(auto& gPin : gCell->gPins()) {
    auto tmpPair = getWireLengthGradientPinWA(gPin, wlCoeffX, wlCoeffY);
    gradientPair.x += tmpPair.x;
    gradientPair.y += tmpPair.y;
  }

  // return sum
  return gradientPair;
}

// get x,y WA Gradient values from GPin
// Please check the JingWei's Ph.D. thesis full paper, 
// Equation (4.13)
//
// You can't understand the following function
// unless you read the (4.13) formula
FloatCoordi
NesterovBase::getWireLengthGradientPinWA(GPin* gPin, float wlCoeffX, float wlCoeffY) {

  float gradientMinX = 0, gradientMinY = 0;
  float gradientMaxX = 0, gradientMaxY = 0;

  // min x
  if( gPin->hasMinExpSumX() ) {
    // from Net.
    float waExpMinSumX = gPin->gNet()->waExpMinSumX();
    float waXExpMinSumX = gPin->gNet()->waXExpMinSumX();

    gradientMinX = 
      ( waExpMinSumX * ( gPin->minExpSumX() * ( 1.0 - wlCoeffX * gPin->cx()) ) 
          + wlCoeffX * gPin->minExpSumX() * waXExpMinSumX )
        / ( waExpMinSumX * waExpMinSumX );
  }
  
  // max x
  if( gPin->hasMaxExpSumX() ) {
    
    float waExpMaxSumX = gPin->gNet()->waExpMaxSumX();
    float waXExpMaxSumX = gPin->gNet()->waXExpMaxSumX();
    
    gradientMaxX = 
      ( waExpMaxSumX * ( gPin->maxExpSumX() * ( 1.0 + wlCoeffX * gPin->cx()) ) 
          - wlCoeffX * gPin->maxExpSumX() * waXExpMaxSumX )
        / ( waExpMaxSumX * waExpMaxSumX );

  }

  // min y
  if( gPin->hasMinExpSumY() ) {
    
    float waExpMinSumY = gPin->gNet()->waExpMinSumY();
    float waYExpMinSumY = gPin->gNet()->waYExpMinSumY();

    gradientMinY = 
      ( waExpMinSumY * ( gPin->minExpSumY() * ( 1.0 - wlCoeffY * gPin->cy()) ) 
          + wlCoeffY * gPin->minExpSumY() * waYExpMinSumY )
        / ( waExpMinSumY * waExpMinSumY );
  }
  
  // max y
  if( gPin->hasMaxExpSumY() ) {
    
    float waExpMaxSumY = gPin->gNet()->waExpMaxSumY();
    float waYExpMaxSumY = gPin->gNet()->waYExpMaxSumY();
    
    gradientMaxY = 
      ( waExpMaxSumY * ( gPin->maxExpSumY() * ( 1.0 + wlCoeffY * gPin->cy()) ) 
          - wlCoeffY * gPin->maxExpSumY() * waYExpMaxSumY )
        / ( waExpMaxSumY * waExpMaxSumY );

  }

  return FloatCoordi(gradientMaxX - gradientMinX, 
      gradientMaxY - gradientMinY);
}

FloatCoordi
NesterovBase::getWireLengthPreconditioner(GCell* gCell) {
  return FloatCoordi( gCell->gPins().size(), 
     gCell->gPins().size() );
}

FloatCoordi
NesterovBase::getDensityPreconditioner(GCell* gCell) {
  float areaVal = static_cast<float>(gCell->dx()) 
    * static_cast<float>(gCell->dy());

  return FloatCoordi(areaVal, areaVal);
}

// Density force cals
void
NesterovBase::updateDensityForceBin() {
  for(auto& bin : bg_.bins()) {
    fft_->updateDensity(bin->x(), bin->y(), 
        bin->density());  
  }
}


void
NesterovBase::reset() { 
  pb_ = nullptr;
  nbVars_.reset();
}


// https://stackoverflow.com/questions/33333363/built-in-mod-vs-custom-mod-function-improve-the-performance-of-modulus-op
static int 
fastModulo(const int input, const int ceil) {
  return input >= ceil? input % ceil : input;
}

static int32_t 
getOverlapArea(Bin* bin, GCell* cell) {
  int rectLx = max(bin->lx(), cell->lx()), 
      rectLy = max(bin->ly(), cell->ly()),
      rectUx = min(bin->ux(), cell->ux()), 
      rectUy = min(bin->uy(), cell->uy());

  if( rectLx >= rectUx || rectLy >= rectUy ) {
    return 0;
  }
  else {
    return static_cast<int32_t>(rectUx - rectLx) 
      * static_cast<int32_t>(rectUy - rectLy);
  }
}

static int32_t 
getOverlapDensityArea(Bin* bin, GCell* cell) {
  int rectLx = max(bin->lx(), cell->dLx()), 
      rectLy = max(bin->ly(), cell->dLy()),
      rectUx = min(bin->ux(), cell->dUx()), 
      rectUy = min(bin->uy(), cell->dUy());

  if( rectLx >= rectUx || rectLy >= rectUy ) {
    return 0;
  }
  else {
    return static_cast<int32_t>(rectUx - rectLx) 
      * static_cast<int32_t>(rectUy - rectLy);
  }
}


static int32_t
getOverlapArea(Bin* bin, Instance* inst) {
  int rectLx = max(bin->lx(), inst->lx()), 
      rectLy = max(bin->ly(), inst->ly()),
      rectUx = min(bin->ux(), inst->ux()), 
      rectUy = min(bin->uy(), inst->uy());

  if( rectLx >= rectUx || rectLy >= rectUy ) {
    return 0;
  }
  else {
    return static_cast<int32_t>(rectUx - rectLx) 
      * static_cast<int32_t>(rectUy - rectLy);
  }
}

static float
fastExp(float a) {
  a = 1.0 * a / 1024.0;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  return a;
}



}
