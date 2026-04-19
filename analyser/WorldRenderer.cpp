#include "WorldRenderer.hpp"

#include "Camera.hpp"
#include "GreedSeet.hpp"
#include "Logs.hpp"
#include "TimeLine.hpp"
#include "VisualisationHelper.hpp"
#include "globals.h"

#include "engine/easy.h"
#include "engine/easy_drawing.h"
#include "engine/easy_sprite.h"
#include "engine/easy_util.h"
#include "engine/font.h"
#include "engine/rgba.h"
#include "engine/vec2d.h"
#include "engine/vec2f.h"
#include "engine/vec2si32.h"

#include <algorithm>
#include <string>
#include <tuple>

using namespace arctic;

extern TimeLine g_time_line;

namespace WorldRenderer {

namespace {

const std::vector<Rgba> kLayersColors = {
};

}  // namespace

void DrawActor(const ActorRec& a) {
  if (!a.visible_) {
    return;
  }
  Vec2F leftDownBlockCorner = g_camera.WorldToScreen(Vec2F(a.offset_));
  Vec2F size = a.visual_size * g_camera.scaleFactor_;
  Vec2F textPos = leftDownBlockCorner + Vec2F(2, 2) * g_camera.scaleFactor_;
  Rgba blockColor = Rgba(128 / 2, 160 / 2, 190 / 2);
  if (a.active_) {
    blockColor = Rgba(255, 160 / 2, 190 / 2);
  }

  Sprite sprite = GetEngine()->GetBackbuffer();
  DrawBlock(sprite,
            leftDownBlockCorner,
            size,
            5.0 * g_camera.scaleFactor_,
            blockColor,
            1.0 * g_camera.scaleFactor_,
            Rgba(0, 0, 0));

  g_font.Draw(sprite, a.text.c_str(), textPos.x, textPos.y,
              kTextOriginBottom, kTextAlignmentLeft,
              kDrawBlendingModeColorize, kFilterNearest,
              Rgba(255, 255, 255));

  Vec2F m = Vec2F(MousePos());
  if (m.x >= leftDownBlockCorner.x
      && m.y >= leftDownBlockCorner.y
      && m.x <= leftDownBlockCorner.x + size.x
      && m.y <= leftDownBlockCorner.y + size.y) {
    g_mouse_nearest_actor_idx = a.id_;
    g_distance_sq_to_nearest_actor = 0;
  } else {
    Vec2F a_center = leftDownBlockCorner + size * 0.5f;
    Vec2F edge = BlockEdgePos(leftDownBlockCorner, a.visual_size, 5.0 * g_camera.scaleFactor_, a_center - Vec2F(MousePos()));
    double distance_sq = LengthSquared(Vec2F(m) - edge);
    if (g_mouse_nearest_actor_idx < 0 ||
        (g_mouse_nearest_message_idx < 0 || distance_sq <= g_distance_sq_to_nearest_message)) {
      if (g_mouse_nearest_actor_idx < 0 || distance_sq <= g_distance_sq_to_nearest_actor) {
        g_mouse_nearest_actor_idx = a.id_;
        g_distance_sq_to_nearest_actor = distance_sq;
      }
    }
  }
}

void UpdateArrow(ArrowRec& arrow) {
  if (arrow.last_update_frame == g_update_frame) {
    return;
  }
  if (arrow.from == arrow.to) {
    const ActorRec& fromActor = g_actors[arrow.from];
    double width = fromActor.visual_size.x;
    double height = fromActor.visual_size.y;
    arrow.points.resize(4);
    arrow.points[0].position = Vec2F(fromActor.offset_) + Vec2F(width / 2 - height / 2, height);
    arrow.points[3].position = arrow.points[0].position + Vec2F(height, 0);
    arrow.points[1].position = arrow.points[0].position + Vec2F(0, 2 * height);
    arrow.points[2].position = arrow.points[3].position + Vec2F(0, 2 * height);
    arrow.points[0].length_part = 0.0;
    arrow.points[1].length_part = 1.0 / 3.0;
    arrow.points[2].length_part = 2.0 / 3.0;
    arrow.points[3].length_part = 1.0;
  } else {
    const ActorRec& fromActor = g_actors[arrow.from];
    const ActorRec& toActor = g_actors[arrow.to];
    Vec2F toCenter = Vec2F(toActor.offset_) + toActor.visual_size * 0.5f;
    Vec2F fromCenter = Vec2F(fromActor.offset_) + fromActor.visual_size * 0.5f;
    arrow.points.resize(2);
    arrow.points[0].position = BlockEdgePos(Vec2F(fromActor.offset_), fromActor.visual_size, 5.0, Vec2F(toCenter - fromCenter));
    arrow.points[1].position = BlockEdgePos(Vec2F(toActor.offset_), toActor.visual_size, 5.0, Vec2F(fromCenter - toCenter));
    arrow.points[0].length_part = 0.0;
    arrow.points[1].length_part = 1.0;
  }
  arrow.last_update_frame = g_update_frame;
}

void DrawArrow(ArrowRec& arrow) {
  if (arrow.last_draw_frame == g_update_frame) {
    return;
  }
  size_t size = arrow.points.size();
  if (size < 2) {
    return;
  }
  Sprite sprite = GetEngine()->GetBackbuffer();
  Vec2F prevPos = g_camera.WorldToScreen(arrow.points[0].position);
  for (size_t i = 1; i < size - 1; ++i) {
    Vec2F pos = g_camera.WorldToScreen(arrow.points[i].position);
    DrawLine(sprite, Vec2Si32(prevPos), Vec2Si32(pos), g_arrow_color);
    prevPos = pos;
  }
  Vec2F pos = g_camera.WorldToScreen(arrow.points[size - 1].position);
  arctic::DrawArrow(sprite, prevPos, pos, g_camera.scaleFactor_, 7 * g_camera.scaleFactor_, 10 * g_camera.scaleFactor_, g_arrow_color);
  arrow.last_draw_frame = g_update_frame;
}

void DrawMessage(ArrowRec& arrow, MessageRec& msg, double d_time, Rgba color) {
  VisualisationTime displayEnd = std::max(msg.end, msg.start + g_min_msg_display_duration);
  double duration = (double)(displayEnd - msg.start);
  double part = (d_time - msg.start) / (duration > 0 ? duration : 1.0);
  Vec2F pos = arrow.PositionAtPart(part);
  Vec2Si32 coord = Vec2Si32(g_camera.WorldToScreen(pos));

  std::string messageType = (std::string)msg.messageType;
  if (VisualisationHelper::IsShortMessageTypeActivate()) {
    messageType = Logs::GetShortLogMessageType(messageType);
  }

  Vec2Si32 messageSize = g_font.EvaluateSize(messageType.c_str(), false);
  Vec2Si32 messagePos = Vec2Si32(coord.x, coord.y - messageSize.y);

  Sprite sprite = GetEngine()->GetBackbuffer();
  DrawRectangle(sprite, messagePos, messagePos + messageSize, Rgba(255, 255, 255));

  DrawCircle(GetEngine()->GetBackbuffer(), coord,
             4.0 * g_camera.GetScaleFactor(),
             Rgba(0, 0, 0));

  DrawCircle(GetEngine()->GetBackbuffer(), coord,
             3.0 * g_camera.GetScaleFactor(),
             color);

  g_font.Draw(sprite, messageType.c_str(), messagePos.x, messagePos.y + 1, kTextOriginBottom, kTextAlignmentLeft,
              kDrawBlendingModeColorize, kFilterNearest,
              Rgba(0, 0, 0));

  double distance_sq = LengthSquared(MousePos() - coord);
  if (g_mouse_nearest_message_idx < 0 || distance_sq <= g_distance_sq_to_nearest_message) {
    g_mouse_nearest_message_idx = msg.id;
    g_distance_sq_to_nearest_message = distance_sq;
  }
}

void DrawArrowAndMessage(MessageRec& m) {
  VisualisationTime displayEnd = std::max(m.end, m.start + g_min_msg_display_duration);
  if (m.start <= g_time_line.GetTime() && displayEnd >= g_time_line.GetTime()) {
    std::tuple<ActorIdx, ActorIdx> fromTo(m.from, m.to);
    ArrowRec& arrow = g_arrows[fromTo];
    UpdateArrow(arrow);

    DrawArrow(arrow);
    DrawMessage(arrow, m, g_time_line.d_time_, VisualisationHelper::GetMessageColor(m.id));
  }
}

void DrawLayers(GreedSeet* gs) {
  const std::vector<ActorTable>& actorTables = gs->actorTables_;
  for (size_t actorTableNum = 0; actorTableNum < actorTables.size(); ++actorTableNum) {
    Rgba layerColor;
    if (actorTableNum < kLayersColors.size()) {
      layerColor = kLayersColors[actorTableNum];
    } else {
      layerColor = Rgba(24, 120, 145);
    }

    std::string layerName;
    if (actorTableNum < g_layer_names.size()) {
      layerName = g_layer_names[actorTableNum];
    } else {
      Check(false, "Unexpected layer");
    }

    Vec2Si32 leftDownCorner(actorTables[actorTableNum].GetXAdd(),
                            actorTables[actorTableNum].GetYAdd() - 1);

    Vec2Si32 rightUpCorner = Vec2Si32(leftDownCorner.x + actorTables[actorTableNum].GetLineLength(),
                                      actorTables[actorTableNum].GetY() + 8);

    Vec2Si32 llc = Vec2Si32(g_camera.WorldToScreen((Vec2F)leftDownCorner));
    Vec2Si32 urc = Vec2Si32(g_camera.WorldToScreen((Vec2F)rightUpCorner));

    Sprite sprite = GetEngine()->GetBackbuffer();
    DrawRectangle(sprite, llc, urc, layerColor);

    g_font.Draw(sprite, layerName.c_str(),
                llc.x, llc.y,
                kTextOriginTop, kTextAlignmentLeft,
                kDrawBlendingModeColorize, kFilterNearest,
                Rgba(255, 255, 255));
  }
}

}  // namespace WorldRenderer
