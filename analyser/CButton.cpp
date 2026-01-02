#include "CButton.hpp"

void CButton::Listen() {
  GetMouse()->Listen();
  
  if (GetMouse()->IsLeftDownward() && IsMouseIn()) {
    Action();
  }
}
