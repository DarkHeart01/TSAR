package middleware

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"

	"endpoint-management-server/internal/auth"
)

const OperatorContextKey = "operator_claims"

// ValidateOperatorJWT is a Gin middleware that enforces operator
// authentication on protected routes.  It validates the Bearer JWT
// signed with the given secret and injects the claims into the
// request context so downstream handlers can read operator_id / username.
func ValidateOperatorJWT(secret string) gin.HandlerFunc {
	return func(c *gin.Context) {
		header := c.GetHeader("Authorization")
		if !strings.HasPrefix(header, "Bearer ") {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "missing operator token"})
			return
		}
		tokenStr := strings.TrimPrefix(header, "Bearer ")

		claims, err := auth.ValidateOperatorJWT(tokenStr, secret)
		if err != nil {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid or expired operator token"})
			return
		}

		c.Set(OperatorContextKey, claims)
		c.Next()
	}
}
