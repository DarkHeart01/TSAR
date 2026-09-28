package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"golang.org/x/crypto/bcrypt"

	"endpoint-management-server/internal/auth"
	"endpoint-management-server/internal/models"
	"endpoint-management-server/internal/repository"
)

// OperatorHandler exposes the operator login endpoint.
// All other operator-facing routes live in PayloadHandler.
type OperatorHandler struct {
	operatorRepo *repository.OperatorRepository
	auditRepo    *repository.AuditRepository
	jwtSecret    string
}

func NewOperatorHandler(
	operatorRepo *repository.OperatorRepository,
	auditRepo *repository.AuditRepository,
	jwtSecret string,
) *OperatorHandler {
	return &OperatorHandler{
		operatorRepo: operatorRepo,
		auditRepo:    auditRepo,
		jwtSecret:    jwtSecret,
	}
}

// Login handles POST /api/v1/operator/login.
// Verifies bcrypt password against the stored hash and returns a
// signed HS256 JWT valid for 8 hours.  Both success and failure are
// written to the audit log so every login attempt is traceable.
func (h *OperatorHandler) Login(c *gin.Context) {
	var req models.OperatorLoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	ctx := c.Request.Context()
	ip := c.ClientIP()

	op, err := h.operatorRepo.GetByUsername(ctx, req.Username)
	if err != nil {
		_ = h.auditRepo.Write(ctx, "operator_login", "operator", nil, ip, map[string]interface{}{
			"username": req.Username,
			"success":  false,
			"reason":   "user not found",
		})
		// Return a generic message to avoid username enumeration.
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid credentials"})
		return
	}

	if err := bcrypt.CompareHashAndPassword([]byte(op.PasswordHash), []byte(req.Password)); err != nil {
		_ = h.auditRepo.Write(ctx, "operator_login", "operator", &op.OperatorID, ip, map[string]interface{}{
			"username": req.Username,
			"success":  false,
			"reason":   "wrong password",
		})
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid credentials"})
		return
	}

	token, expiry, err := auth.IssueOperatorJWT(op.OperatorID, op.Username, h.jwtSecret)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to issue token"})
		return
	}

	_ = h.auditRepo.Write(ctx, "operator_login", "operator", &op.OperatorID, ip, map[string]interface{}{
		"username": req.Username,
		"success":  true,
	})

	c.JSON(http.StatusOK, models.OperatorLoginResponse{Token: token, ExpiresAt: expiry})
}
