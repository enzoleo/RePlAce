#include "routeBase.h"
#include "logger.h"
#include "placerBase.h"
#include <opendb/db.h>
#include <string>
#include <iostream>
#include <cmath>

using std::vector;
using std::string;

namespace replace {

Tile::Tile()
: x_(0), y_(0), 
  lx_(0), ly_(0), ux_(0), uy_(0),
  sumUsageH_(0), sumUsageV_(0),
  supplyH_(0), supplyV_(0),
  supplyHL_(0), supplyHR_(0),
  supplyVL_(0), supplyVR_(0), 
  inflationRatioH_(0),
  inflationRatioV_(0),
  inflationRatio_(0),
  inflationArea_(0),
  inflationAreaDelta_(0),
  inflatedRatio_(0), 
  isMacroIncluded_(false) {}

Tile::Tile(int x, int y, int lx, int ly, int ux, int uy, int layers) 
: Tile() {
  x_ = x;
  y_ = y;
  lx_ = lx;
  ly_ = ly;
  ux_ = ux; 
  uy_ = uy;

  blockage_.resize(layers, 0);
  capacity_.resize(layers, 0);
  route_.resize(layers, 0);

  usageHL_.resize(layers, 0);
  usageHR_.resize(layers, 0);
  usageVL_.resize(layers, 0);
  usageVR_.resize(layers, 0);
}

Tile::~Tile() {
  reset();
}

void
Tile::reset() {
  x_ = y_ = lx_ = ly_ = ux_ = uy_ = 0;
  blockage_.clear();
  capacity_.clear();
  route_.clear();

  usageHL_.clear();
  usageHR_.clear();
  usageVL_.clear();
  usageVR_.clear();
  
  usageHL_.shrink_to_fit();
  usageHR_.shrink_to_fit();
  usageVL_.shrink_to_fit();
  usageVR_.shrink_to_fit();
}

int
Tile::blockage(int layer) const {
  return blockage_[layer];
}

int
Tile::capacity(int layer) const {
  return capacity_[layer];
}

int
Tile::route(int layer) const {
  return route_[layer];
}

int
Tile::usageHL(int layer) const {
  return usageHL_[layer];
}

int
Tile::usageHR(int layer) const {
  return usageHR_[layer];
}

int
Tile::usageVL(int layer) const {
  return usageVL_[layer];
}

int 
Tile::usageVR(int layer) const {
  return usageVR_[layer];
}

void
Tile::setBlockage(int layer, int block) {
  blockage_[layer] = block;
}

void
Tile::setCapacity(int layer, int capacity) {
  capacity_[layer] = capacity;
}

void
Tile::setCapacity(vector<int>& capacity) {
  capacity_ = capacity; 
}

void
Tile::setRoute(int layer, int route) {
  route_[layer] = route;
}

void
Tile::setUsageHL(int layer, int usage) { 
  usageHL_[layer] = usage;
}

void
Tile::setUsageHR(int layer, int usage) { 
  usageHR_[layer] = usage;
}

void
Tile::setUsageVL(int layer, int usage) { 
  usageVL_[layer] = usage;
}

void
Tile::setUsageVR(int layer, int usage) { 
  usageVR_[layer] = usage;
}

float 
Tile::sumUsageH() const {
  return sumUsageH_;
}

float
Tile::sumUsageV() const {
  return sumUsageV_; 
}

float
Tile::supplyHL() const {
  return supplyHL_;
}
float
Tile::supplyHR() const {
  return supplyHR_;
}

float
Tile::supplyVL() const {
  return supplyVL_;
}
float
Tile::supplyVR() const {
  return supplyVR_;
}

void
Tile::setSupplyH(float supply) {
  supplyH_ = supply;
}

void
Tile::setSupplyV(float supply) {
  supplyV_ = supply;
}

// set func for supply*
void
Tile::setSupplyHL(float supply) {
  supplyHL_ = supply;
}

void
Tile::setSupplyHR(float supply) {
  supplyHR_ = supply;
}

void
Tile::setSupplyVL(float supply) {
  supplyVL_ = supply; 
}

void
Tile::setSupplyVR(float supply) {
  supplyVR_ = supply; 
}

void
Tile::setMacroIncluded(bool mode) {
  isMacroIncluded_ = mode;
}



TileGrid::TileGrid()
  : db_(nullptr), 
  lx_(0), ly_(0), 
  tileCntX_(0), tileCntY_(0), 
  tileSizeX_(0), tileSizeY_(0),
  tileNumLayers_(0),
  blockagePorosity_(0), gRoutePitchScale_(1.08) {}

TileGrid::~TileGrid() {
  reset();
}

void
TileGrid::reset() {
  db_ = nullptr;
  lx_ = ly_ = 0; 
  tileCntX_ = tileCntY_ = 0;
  tileSizeX_ = tileSizeY_ = 0;
  tileNumLayers_ = 0;
  gRoutePitchScale_ = 1.08;

  tileStor_.clear();
  tiles_.clear();

  tileStor_.shrink_to_fit();
  tiles_.shrink_to_fit();
}

void
TileGrid::setDb(odb::dbDatabase* db) {
  db_ = db;
}

void
TileGrid::setLogger(std::shared_ptr<Logger> log) {
  log_ = log;
}

void
TileGrid::setTileCnt(int tileCntX, int tileCntY) {
  tileCntX_ = tileCntX;
  tileCntY_ = tileCntY;
}

void
TileGrid::setTileCntX(int tileCntX) {
  tileCntX_ = tileCntX;
}

void
TileGrid::setTileCntY(int tileCntY) {
  tileCntY_ = tileCntY;  
}

void
TileGrid::setTileSize(int tileSizeX, int tileSizeY) {
  tileSizeX_ = tileSizeX;
  tileSizeY_ = tileSizeY;
}

void
TileGrid::setTileSizeX(int tileSizeX) {
  tileSizeX_ = tileSizeX;
}

void
TileGrid::setTileSizeY(int tileSizeY) {
  tileSizeY_ = tileSizeY; 
}

void
TileGrid::initTiles() {

  log_->infoIntPair("TileLxLy", lx_, ly_);
  log_->infoIntPair("TileSize", tileSizeX_, tileSizeY_);
  log_->infoIntPair("TileCnt", tileCntX_, tileCntY_);

  assert( tileNumLayers_ 
      == db_->getTech()->getRoutingLayerCount() );

  int numHTracks = 0;
  int numVTracks = 0;
  for(int i=0; i<tileNumLayers_; i++) {
    numVTracks += 
      verticalCapacity_[i] 
      / (minWireWidth_[i] + minWireSpacing_[i]);
    numHTracks +=
      horizontalCapacity_[i] 
      / (minWireWidth_[i] + minWireSpacing_[i]);
  }

  log_->infoInt("NumHTracks", numHTracks);
  log_->infoInt("NumVTracks", numVTracks);

  // A bit curious why gRoutePitchScale is set as 1.08 ????
  //
  int pitchH = std::round(static_cast<float>(tileSizeY_) 
      / numHTracks * gRoutePitchScale_);
  int pitchV = std::round(static_cast<float>(tileSizeX_) 
      / numVTracks * gRoutePitchScale_);

  log_->infoInt("PitchH", pitchH );
  log_->infoInt("PitchV", pitchV );

  // 2D tile grid structure init
  int x = lx_, y = ly_;
  int idxX = 0, idxY = 0;
  tileStor_.resize(tileCntX_ * tileCntY_);
  for(auto& tile : tileStor_) {
    tile = Tile(idxX, idxY, x, y, 
        x + tileSizeX_, y + tileSizeY_,
        tileNumLayers_); 

    x += tileSizeX_;
    idxX += 1;
    if( x >= ux() ) {
      y += tileSizeY_;
      x = lx_;

      idxY ++;
      idxX = 0;
    }

    tile.setSupplyHL( tile.area() / pitchH );
    tile.setSupplyHR( tile.supplyHL() );

    tile.setSupplyVL( tile.area() / pitchV );
    tile.setSupplyVR( tile.supplyVL() );
  
    tiles_.push_back( &tile );
  }
  log_->infoInt("NumTiles", tiles_.size());

  // apply edgeCapacityInfo from *.route
  for(auto& ecInfo : edgeCapacityStor_) {
    bool isHorizontal = (ecInfo.ly == ecInfo.uy);
    
    // l : lower
    // u : upper
    int lx = std::min( ecInfo.lx, ecInfo.ux );
    int ux = std::max( ecInfo.lx, ecInfo.ux );
    int ly = std::min( ecInfo.ly, ecInfo.uy );
    int uy = std::max( ecInfo.ly, ecInfo.uy );

    // Note that ecInfo.ll == ecInfo.ul
    assert( ecInfo.ll == ecInfo.ul );
    int layer = ecInfo.ll;
    int capacity = ecInfo.capacity;

    Tile* lTile = tiles()[lx * tileCntY() + ly];
    Tile* uTile = tiles()[ux * tileCntY() + uy];

    if( isHorizontal ) {
      // lower -> right edge
      lTile->setSupplyHR( 
          lTile->supplyHR() - 
          (horizontalCapacity_[layer - 1] - capacity) /
          (minWireWidth_[layer - 1] + minWireSpacing_[layer - 1]) /
          tileSizeX_ );

      // upper -> left edge
      uTile->setSupplyHL(
          uTile->supplyHL() -
          (horizontalCapacity_[layer - 1] - capacity) /
          (minWireWidth_[layer - 1] + minWireSpacing_[layer - 1]) /
          tileSizeX_ );

      // lower layer check
      if( layer <= 5 && horizontalCapacity_[layer - 1] > 0 &&
          capacity < 0.01 ) {
        lTile->setMacroIncluded(true);
      }
    }
    else {
      // lower -> right edge
      lTile->setSupplyVR( 
          lTile->supplyVR() - 
          (verticalCapacity_[layer - 1] - capacity) /
          (minWireWidth_[layer - 1] + minWireSpacing_[layer - 1]) /
          tileSizeY_ );

      // upper -> left edge
      uTile->setSupplyVL(
          uTile->supplyVL() -
          (verticalCapacity_[layer - 1] - capacity) /
          (minWireWidth_[layer - 1] + minWireSpacing_[layer - 1]) /
          tileSizeY_ );

      // lower layer check
      if( layer <= 5 && verticalCapacity_[layer - 1] > 0 &&
          capacity < 0.01 ) {
        lTile->setMacroIncluded(true);
      }
    }
  } 
  
  // fill capacity
  std::vector<int> capacity(tileNumLayers_, 0);
  for(int i=0; i<tileNumLayers_; i++) {
    if( horizontalCapacity_[i] > 0 ) {
      capacity[i] = horizontalCapacity_[i]; 
    }
    else {
      capacity[i] = verticalCapacity_[i];
    }
  }

  for(auto& tile : tiles_) {
    // set H, V from L, R
    tile->setSupplyH( std::fmin( tile->supplyHL(), tile->supplyHR() ) );
    tile->setSupplyV( std::fmin( tile->supplyVL(), tile->supplyVR() ) );

    // set capacity initially
    tile->setCapacity( capacity );
  }

}

int
TileGrid::lx() const {
  return lx_; 
}

int 
TileGrid::ly() const {
  return ly_;
}

int 
TileGrid::ux() const {
  return lx_ + tileCntX_ * tileSizeX_;
}
int 
TileGrid::uy() const {
  return ly_ + tileCntY_ * tileSizeY_;
}

int
TileGrid::tileCntX() const {
  return tileCntX_;
}

int
TileGrid::tileCntY() const {
  return tileCntY_; 
}

int
TileGrid::tileSizeX() const {
  return tileSizeX_;
}

int
TileGrid::tileSizeY() const {
  return tileSizeY_;
}


// fill 
// lx_ ly_ 
// tileCntX_ tileCntY_
// tileSizeX_ tileSizeY_ 
//
// blockagePorosity_
//
// verticalCapacity_
// horizontalCapacity_
//
// minWireWidth_
// minWireSpacing_
//
// edgeCapacityStor_
void
TileGrid::initFromRoute(const char* fileName) {
  char *token = NULL;
  char temp[255];
  char line[255];
  bool blockageFlag = false;
  bool beolFlag = false;
  bool edgeFlag = false;

  FILE *fp = nullptr;
  if((fp = fopen(fileName, "r")) == NULL) {
    log_->error("Cannot open " + string(fileName) + " file!");
    exit(1);
  }

  while(!feof(fp)) {
    *line = '\0';
    char* ptr = fgets(line, 255, fp);
    sscanf(line, "%s%*s", temp);

    if(strlen(line) < 5 || temp[0] == '#' || strcmp(temp, "route") == 0)
      continue;
    if(strcmp(temp, "NumBlockageNodes") == 0) {
      blockageFlag = true;
      continue;
    }
    if(strcmp(temp, "NumEdgeCapacityAdjustments") == 0) {
      blockageFlag = false;
      edgeFlag = true;
      continue;
    }
    if(strcmp(temp, "Grid") == 0) {
      beolFlag = true;
    }
    if(strcmp(temp, "NumNiTerminals") == 0) {
      beolFlag = false;
      continue;
    }

    if(beolFlag) {
      sscanf(line, "%s :%*s", temp);
      if(strcmp(temp, "Grid") == 0) {
        sscanf(line, "%*s : %d %d %d", &tileCntX_, &tileCntY_, &tileNumLayers_);
      }
      else if(strcmp(temp, "VerticalCapacity") == 0) {
        verticalCapacity_.clear();
        token = strtok(line, " \t\n");
        token = strtok(NULL, " \t\n");
        token = strtok(NULL, " \t\n");
        for(int i = 0; i < tileNumLayers_; i++) {
          verticalCapacity_.push_back(atoi(token));
          token = strtok(NULL, " \t\n");
        }
      }
      else if(strcmp(temp, "HorizontalCapacity") == 0) {
        horizontalCapacity_.clear();
        token = strtok(line, " \t\n");
        token = strtok(NULL, " \t\n");
        token = strtok(NULL, " \t\n");
        for(int i = 0; i < tileNumLayers_; i++) {
          horizontalCapacity_.push_back(atoi(token));
          token = strtok(NULL, " \t\n");
        }
      }
      else if(strcmp(temp, "MinWireWidth") == 0) {
        minWireWidth_.clear();
        token = strtok(line, " \t\n");
        token = strtok(NULL, " \t\n");
        token = strtok(NULL, " \t\n");
        for(int i = 0; i < tileNumLayers_; i++) {
          minWireWidth_.push_back(atof(token));
          token = strtok(NULL, " \t\n");
        }
      }
      else if(strcmp(temp, "MinWireSpacing") == 0) {
        minWireSpacing_.clear();
        token = strtok(line, " \t\n");
        token = strtok(NULL, " \t\n");
        token = strtok(NULL, " \t\n");
        for(int i = 0; i < tileNumLayers_; i++) {
          minWireSpacing_.push_back(atof(token));
          token = strtok(NULL, " \t\n");
        }
      }
      else if(strcmp(temp, "ViaSpacing") == 0) {
        token = strtok(line, " \t\n");
        token = strtok(NULL, " \t\n");
        token = strtok(NULL, " \t\n");
        for(int i = 0; i < tileNumLayers_; i++) {
          token = strtok(NULL, " \t\n");
        }
      }
      else if(strcmp(temp, "GridOrigin") == 0) {
        double temp_gridLLx, temp_gridLLy;
        sscanf(line, "%*s : %lf %lf", &temp_gridLLx, &temp_gridLLy);
        lx_ = temp_gridLLx;
        ly_ = temp_gridLLy;
      }
      else if(strcmp(temp, "TileSize") == 0) {
        double temp_tileWidth, temp_tileHeight;
        sscanf(line, "%*s : %lf %lf", &temp_tileWidth, &temp_tileHeight);
        tileSizeX_ = temp_tileWidth;
        tileSizeY_ = temp_tileHeight;
      }
      else if(strcmp(temp, "BlockagePorosity") == 0) {
        double temp_blockagePorosity;
        sscanf(line, "%*s : %lf", &temp_blockagePorosity);
        blockagePorosity_ = (float)temp_blockagePorosity;
      }
    }


    // No need to care
    if(blockageFlag) {
    }

    if(edgeFlag) {
      int e1, e2, e3, e4, e5, e6, e7;
      sscanf(line, "%d %d %d %d %d %d %d ", &e1, &e2, &e3, &e4, &e5, &e6, &e7);
      edgeCapacityStor_.push_back(EdgeCapacityInfo(e1, e2, e3, e4, e5, e6, e7));
//      using std::cout;
//      using std::endl;
//      cout << e1<< " " << e2 << " " << e3 << " ";
//      cout << e4 << " " << e5 << " " << e6 << " " << e7 << endl;
    }
  }


  // init Tiles
  initTiles();
}

// Fill routingTracks_;
void 
TileGrid::importEst(const char* fileName) {

}




EdgeCapacityInfo::EdgeCapacityInfo()
: lx(0), ly(0), ll(0), ux(0), uy(0), ul(0), capacity(0) {}

EdgeCapacityInfo::EdgeCapacityInfo(int lx1, int ly1, int ll1,
  int ux1, int uy1, int ul1, int capacity1)
  : lx(lx1), ly(ly1), ll(ll1), ux(ux1), uy(uy1), ul(ul1), capacity(capacity1) {}

// RoutingTrack structure
RoutingTrack::RoutingTrack() 
: lx(0), ly(0), ux(0), uy(0), layer(0), gNet(nullptr) {}

RoutingTrack::RoutingTrack(int lx1, int ly1, int ux1, int uy1, int layer1, GNet* gNet1) 
: RoutingTrack() {
  lx = lx1;
  ly = ly1;
  ux = ux1;
  uy = uy1;
  layer = layer1;
  gNet = gNet1;
}




RouteBase::RouteBase()
  : db_(nullptr), nb_(nullptr), log_(nullptr) {}

RouteBase::RouteBase(
    odb::dbDatabase* db, 
    std::shared_ptr<NesterovBase> nb,
    std::shared_ptr<Logger> log)
  : RouteBase() {
  db_ = db;
  nb_ = nb;
  log_ = log;

  init();
}

RouteBase::~RouteBase() {
  reset();
}

void 
RouteBase::reset() {
  db_ = nullptr;
  nb_ = nullptr;
  log_ = nullptr;
}

void
RouteBase::init() {
  initFromRoute("input.route");
  log_->infoString("input.route parsing is done");
}


// will be removed in the near future
void
RouteBase::initFromRoute(const char* fileName) {
  tg_.setDb(db_);
  tg_.setLogger(log_);
  tg_.initFromRoute(fileName);
}

void
RouteBase::importEst(const char* fileName) {
  tg_.importEst(fileName);
}

}