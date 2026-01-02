#include "Drawer.hpp"
#include "IDrawElement.hpp"
#include <stdexcept>

void Drawer::Draw() const {

  for (IDrawElement* drawElement : drawStorage_) {
    if (!drawElement) {
      throw std::runtime_error("Null pointer, Draw: drawElement");
    }

    drawElement->Draw(this);
  }

  const Window* curWindow = GetWindow();
  for (Window* window : curWindow->GetSubWindows()) {
    Drawer* drawer = dynamic_cast<Drawer *>(window);
    drawer->Draw();
  }
}

void Drawer::AddDrawElement(IDrawElement *drawElement) {
  if (!drawElement) {
    throw std::runtime_error("Null pointer, Add: drawElement");
  }
  drawStorage_.push_back(drawElement);
}
